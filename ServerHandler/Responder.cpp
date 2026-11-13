#include "Server.hpp"

namespace
{
    const size_t MAX_COALESCED_OUTPUT = 64 * 1024;

    struct OutputSegment
    {
        std::string bytes;
        size_t offset;
        int fileFd;
        off_t fileRemaining;
        bool isFile;

        OutputSegment(const std::string& data)
            : bytes(data), offset(0), fileFd(-1), fileRemaining(0), isFile(false) {}

        OutputSegment(std::string&& data)
            : bytes(std::move(data)), offset(0), fileFd(-1), fileRemaining(0), isFile(false) {}

        OutputSegment(int fd, off_t size)
            : offset(0), fileFd(fd), fileRemaining(size), isFile(true) {}
    };

    struct ClientOutput
    {
        std::deque<OutputSegment> segments;
        bool closeWhenDrained;

        ClientOutput() : closeWhenDrained(false) {}
    };

    enum OutputFlushStep
    {
        OUTPUT_FLUSH_ERROR,
        OUTPUT_FLUSH_CONTINUE,
        OUTPUT_FLUSH_PAUSE
    };

    std::map<int, ClientOutput> clientOutputs;

    bool loadFileChunk(OutputSegment& segment)
    {
        if (!segment.isFile || segment.offset != segment.bytes.size()
            || segment.fileRemaining <= 0)
            return true;

        char buffer[16384];
        size_t amount = sizeof(buffer);
        if (segment.fileRemaining < static_cast<off_t>(amount))
            amount = static_cast<size_t>(segment.fileRemaining);

        ssize_t bytesRead = read(segment.fileFd, buffer, amount);
        if (bytesRead <= 0)
            return false;

        segment.bytes.assign(buffer, static_cast<size_t>(bytesRead));
        segment.offset = 0;
        segment.fileRemaining -= bytesRead;
        return true;
    }

    bool getCoalescedBatchSize(const ClientOutput& output,
                               size_t& batchSize, size_t& batchCount)
    {
        batchSize = 0;
        batchCount = 0;

        for (std::deque<OutputSegment>::const_iterator it =
                 output.segments.begin();
             it != output.segments.end() && !it->isFile; ++it)
        {
            size_t available = it->bytes.size() - it->offset;
            if (available > MAX_COALESCED_OUTPUT - batchSize)
            {
                batchSize = 0;
                batchCount = 0;
                return false;
            }
            batchSize += available;
            ++batchCount;
        }
        return true;
    }

    std::string makeCoalescedBatch(const ClientOutput& output,
                                   size_t batchSize)
    {
        std::string batch;
        batch.reserve(batchSize);
        for (std::deque<OutputSegment>::const_iterator it =
                 output.segments.begin();
             it != output.segments.end() && !it->isFile; ++it)
            batch.append(it->bytes, it->offset, it->bytes.size() - it->offset);
        return batch;
    }

    void consumeCoalescedBytes(ClientOutput& output, size_t sentBytes)
    {
        while (sentBytes > 0 && !output.segments.empty())
        {
            OutputSegment& segment = output.segments.front();
            size_t available = segment.bytes.size() - segment.offset;
            if (sentBytes >= available)
            {
                sentBytes -= available;
                output.segments.pop_front();
            }
            else
            {
                segment.offset += sentBytes;
                sentBytes = 0;
            }
        }
    }

    OutputFlushStep sendCoalescedBatch(int clientFd, ClientOutput& output,
                                       size_t batchSize)
    {
        std::string batch = makeCoalescedBatch(output, batchSize);
        ssize_t sent = send(clientFd, batch.data(), batch.size(), 0);
        if (sent < 0)
            return OUTPUT_FLUSH_PAUSE;
        if (sent == 0)
            return OUTPUT_FLUSH_ERROR;

        consumeCoalescedBytes(output, static_cast<size_t>(sent));
        return OUTPUT_FLUSH_PAUSE;
    }

    OutputFlushStep sendFrontSegment(int clientFd, ClientOutput& output)
    {
        OutputSegment& segment = output.segments.front();
        ssize_t sent = send(clientFd, segment.bytes.data() + segment.offset,
                            segment.bytes.size() - segment.offset, 0);
        if (sent < 0)
            return OUTPUT_FLUSH_PAUSE;
        if (sent == 0)
            return OUTPUT_FLUSH_ERROR;

        segment.offset += static_cast<size_t>(sent);
        if (segment.offset < segment.bytes.size())
            return OUTPUT_FLUSH_PAUSE;

        if (segment.isFile)
        {
            if (segment.fileRemaining == 0)
            {
                if (segment.fileFd != -1)
                    close(segment.fileFd);
                output.segments.pop_front();
            }
            return OUTPUT_FLUSH_CONTINUE;
        }

        output.segments.pop_front();
        return OUTPUT_FLUSH_PAUSE;
    }

    void removeEmptyFrontSegment(ClientOutput& output)
    {
        OutputSegment& segment = output.segments.front();
        if (!segment.isFile || segment.fileRemaining == 0)
        {
            if (segment.isFile && segment.fileFd != -1)
                close(segment.fileFd);
            output.segments.pop_front();
        }
    }

    bool targetHasExtension(const std::string& target,
                            const std::string& extension)
    {
        const size_t queryPosition = target.find('?');
        const std::string path = target.substr(0, queryPosition);
        return path.size() >= extension.size()
            && path.compare(path.size() - extension.size(),
                            extension.size(), extension) == 0;
    }
}

std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

std::string statusText(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Content Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Method Not Implemented";
        case 505: return "HTTP Version Not Supported";
        default: return "";
    }
}

std::string allowHeaderValue(const ServerConfig& config,
                             const LocationConfig* location)
{
    std::vector<std::string> methods;
    if (location != NULL && !location->allowed_methods.empty())
        methods = location->allowed_methods;
    else if (!config.allowed_methods.empty())
    {
        for (size_t i = 0; i < config.allowed_methods.size(); ++i)
        {
            if (config.allowed_methods[i] == GET)
                methods.push_back("GET");
            else if (config.allowed_methods[i] == POST)
                methods.push_back("POST");
            else if (config.allowed_methods[i] == DELETE)
                methods.push_back("DELETE");
        }
    }
    else
        methods.push_back("GET");

    std::string value;
    for (size_t i = 0; i < methods.size(); ++i)
    {
        if (!value.empty())
            value += ", ";
        value += methods[i];
    }
    return value;
}

std::string findHeaderValue(const std::map<std::string, std::string>& headers, const std::string& name)
{
    std::string lname = toLower(name);
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
        if (toLower(it->first) == lname)
            return it->second;
    return std::string();
}

ssize_t sendAll(int fd, const char* buf, size_t len)
{
    if (len == 0)
        return 0;
    clientOutputs[fd].segments.push_back(
        OutputSegment(std::string(buf, len)));
    return static_cast<ssize_t>(len);
}

bool hasPendingOutput(int clientFd)
{
    std::map<int, ClientOutput>::const_iterator it =
        clientOutputs.find(clientFd);
    return it != clientOutputs.end() && !it->second.segments.empty();
}

bool shouldCloseAfterOutput(int clientFd)
{
    std::map<int, ClientOutput>::const_iterator it =
        clientOutputs.find(clientFd);
    return it != clientOutputs.end() && it->second.closeWhenDrained;
}

void closeAfterPendingOutput(int clientFd)
{
    clientOutputs[clientFd].closeWhenDrained = true;
}

void clearPendingOutput(int clientFd)
{
    std::map<int, ClientOutput>::iterator it = clientOutputs.find(clientFd);
    if (it == clientOutputs.end())
        return;
    for (size_t i = 0; i < it->second.segments.size(); ++i)
        if (it->second.segments[i].isFile && it->second.segments[i].fileFd != -1)
            close(it->second.segments[i].fileFd);
    clientOutputs.erase(it);
}

bool flushPendingOutput(int clientFd)
{
    std::map<int, ClientOutput>::iterator output = clientOutputs.find(clientFd);
    if (output == clientOutputs.end())
        return true;

    while (!output->second.segments.empty())
    {
        OutputSegment& segment = output->second.segments.front();
        if (!loadFileChunk(segment))
            return false;

        if (segment.offset < segment.bytes.size())
        {
            if (!segment.isFile)
            {
                size_t batchSize = 0;
                size_t batchCount = 0;
                getCoalescedBatchSize(output->second, batchSize, batchCount);
                if (batchCount > 1)
                {
                    OutputFlushStep step = sendCoalescedBatch(
                        clientFd, output->second, batchSize);
                    return step != OUTPUT_FLUSH_ERROR;
                }
            }

            OutputFlushStep step = sendFrontSegment(clientFd, output->second);
            if (step == OUTPUT_FLUSH_ERROR)
                return false;
            if (step == OUTPUT_FLUSH_PAUSE)
                return true;
            continue;
        }

        removeEmptyFrontSegment(output->second);
    }
    return true;
}

std::string normalizeTarget(const std::string& rawTarget)
{
    std::string target = rawTarget;
    const size_t qpos = target.find('?');
    const size_t hashPos = target.find('#');

    if (qpos != std::string::npos)
        target = target.substr(0, qpos);
    if (hashPos != std::string::npos)
        target = target.substr(0, hashPos);
    if (target.empty() || target[0] != '/')
        target = "/" + target;
    return target;
}

std::string resolveRequestedPath(const std::string& root, const std::string& target)
{
    std::string baseRoot = root;
    if (baseRoot.empty())
        baseRoot = ".";
    if (!baseRoot.empty() && baseRoot.back() == '/')
        baseRoot.pop_back();

    std::string path = baseRoot + target;
    if (!path.empty() && path.back() == '/')
        path += "index.html";
    return path;
}

bool sendFileBody(int clientFd, int fd_in, off_t fileSize)
{
    if (fileSize <= 0)
    {
        close(fd_in);
        return true;
    }
    clientOutputs[clientFd].segments.push_back(OutputSegment(fd_in, fileSize));
    return true;
}

bool queueCgiResponse(int clientFd, const CgiJob& job, CGIResult& result,
                      const ServerConfig* config)
{
    if (result.timed_out || result.io_error || result.exit_status != 0)
        return sendErrorResponse(clientFd, job.httpVersion, job.keepAlive,
                                 500, config);

    CgiResponse parsed;
    if (!parsed.parse(result.raw_output))
        return sendErrorResponse(clientFd, job.httpVersion, job.keepAlive,
                                 500, config);
    std::string().swap(result.raw_output);

    std::ostringstream response;
    response << job.httpVersion << " " << parsed.status_code << " "
             << parsed.status_message << "\r\n";
    for (std::map<std::string, std::string>::const_iterator it =
             parsed.headers.begin(); it != parsed.headers.end(); ++it)
    {
        if (it->first == "content-length" || it->first == "connection")
            continue;
        response << it->first << ": " << it->second << "\r\n";
    }
    response << "Content-Length: " << parsed.body.size() << "\r\n";
    if (job.keepAlive)
        response << "Connection: keep-alive\r\n";
    else
        response << "Connection: close\r\n";
    response << "\r\n";

    ClientOutput& output = clientOutputs[clientFd];
    output.segments.push_back(OutputSegment(response.str()));
    output.segments.push_back(OutputSegment(std::move(parsed.body)));
    return true;
}

std::string toUpper(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(value[i])));
    return value;
}

namespace
{
    void setKeepAlive(const HttpParser& http, bool& keepAlive)
    {
        std::string version = http.getVersion();
        std::string connection =
            toLower(findHeaderValue(http.getHeaders(), "connection"));
        if (connection == "close")
            keepAlive = false;
        else if (connection == "keep-alive")
            keepAlive = true;
        else
            keepAlive = (version == "HTTP/1.1");
    }

    bool sendRedirectResponse(const HttpParser& http, int clientFd,
                             bool keepAlive, const LocationConfig& location)
    {
        std::ostringstream response;
        response << http.getVersion() << " " << location.redirectCode << " "
                 << statusText(location.redirectCode) << "\r\n"
                 << "Location: " << location.redirectTarget << "\r\n"
                 << "Content-Length: 0\r\n";
        if (keepAlive)
            response << "Connection: keep-alive\r\n";
        else
            response << "Connection: close\r\n";
        response << "\r\n";

        const std::string message = response.str();
        return sendAll(clientFd, message.data(), message.size()) != -1;
    }

    bool isConfiguredCgiRoute(const LocationConfig* location,
                              const std::string& target)
    {
        if (location == NULL)
            return false;
        if (location->path.compare(0, 2, "*.") != 0)
            return false;
        if (location->cgi_path.empty())
            return false;
        return targetHasExtension(target, location->path.substr(1));
    }

    CGIRequest makeCgiRequest(HttpParser& http, const ServerConfig& config,
                              const LocationConfig* location,
                              bool configuredCgi,
                              const std::string& version)
    {
        CGIRequest request;
        std::string target = http.getTarget();
        size_t queryPosition = target.find('?');
        std::string scriptUri = target;
        if (queryPosition != std::string::npos)
            scriptUri = target.substr(0, queryPosition);

        request.method = toUpper(http.getMethod());
        if (configuredCgi)
        {
            request.script_name.clear();
            request.path_info = scriptUri;
            request.interpreter = location->cgi_path;
        }
        else
            request.script_name = scriptUri;

        const std::string* root = &config.root;
        if (configuredCgi && !location->root.empty())
            root = &location->root;

        std::string scriptPathUri = scriptUri;
        if (configuredCgi)
        {
            size_t lastSlash = scriptUri.find_last_of('/');
            if (lastSlash != std::string::npos)
                scriptPathUri = scriptUri.substr(lastSlash + 1);
        }

        request.script_path = resolveRequestedPath(*root, scriptPathUri);
        if (configuredCgi)
            request.path_translated = resolveRequestedPath(*root, scriptUri);

        if (queryPosition != std::string::npos)
            request.query_string = target.substr(queryPosition + 1);

        request.body = http.takeBody();
        request.headers = http.getHeaders();
        request.server_protocol = version;
        request.server_name = findHeaderValue(http.getHeaders(), "host");
        request.server_software = "webserv/1.0";
        request.server_port = http.getPortNum();
        return request;
    }

    bool startCgiResponse(HttpParser& http, int clientFd, bool keepAlive,
                          const ServerConfig& config, CgiHandler& cgi,
                          std::map<int, CgiJob>& cgiJobs,
                          const LocationConfig* location,
                          bool configuredCgi, const std::string& version)
    {
        if (configuredCgi
            && !isMethodAllowed(config, location, toUpper(http.getMethod())))
        {
            std::vector<std::pair<std::string, std::string> > extraHeaders;
            extraHeaders.push_back(std::make_pair(
                "Allow", allowHeaderValue(config, location)));
            return sendErrorResponse(clientFd, version, keepAlive, 405,
                                     &config, extraHeaders);
        }

        CGIRequest request = makeCgiRequest(
            http, config, location, configuredCgi, version);
        CgiJob job;
        unsigned int timeoutSeconds = 5;
        if (request.body.size() >= 8 * 1024 * 1024)
            timeoutSeconds = 30;
        if (!cgi.start(request, job, timeoutSeconds))
            return sendErrorResponse(clientFd, version, keepAlive, 500,
                                     &config);

        job.clientFd = clientFd;
        job.httpVersion = version;
        job.keepAlive = keepAlive;
        cgiJobs.emplace(clientFd, std::move(job));
        return true;
    }

    bool dispatchResponse(HttpParser& http, int clientFd, bool keepAlive,
                          const ServerConfig& config)
    {
        std::string method = toLower(http.getMethod());
        if (method == "get")
        {
            handleGet(http, http.getVersion(), keepAlive, clientFd, config);
            return true;
        }
        if (method == "post")
        {
            handlePost(http, http.getVersion(), keepAlive, clientFd, config);
            return true;
        }
        if (method == "delete")
        {
            handleDelete(clientFd, http.getVersion(), keepAlive,
                         http.getTarget(), config);
            return true;
        }
        return false;
    }
}

bool buildResponse(HttpParser& http, int clientFd,
                   bool& keepAlive, const ServerConfig& config,
                   CgiHandler& cgi, std::map<int, CgiJob>& cgiJobs)
{
    std::string version = http.getVersion();
    setKeepAlive(http, keepAlive);

    const LocationConfig* location =
        findMatchingLocation(config, http.getTarget(), toUpper(http.getMethod()));
    if (location != NULL && location->redirectCode != 0)
        return sendRedirectResponse(http, clientFd, keepAlive, *location);

    bool configuredCgi =
        isConfiguredCgiRoute(location, http.getTarget());

    if (configuredCgi || cgi.isCGI(http.getTarget()))
        return startCgiResponse(http, clientFd, keepAlive, config, cgi,
                                cgiJobs, location, configuredCgi, version);

    return dispatchResponse(http, clientFd, keepAlive, config);
}
