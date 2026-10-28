#include "Server.hpp"

namespace
{
    struct OutputSegment
    {
        std::string bytes;
        size_t offset;
        int fileFd;
        off_t fileRemaining;
        bool isFile;

        OutputSegment(const std::string& data)
            : bytes(data), offset(0), fileFd(-1), fileRemaining(0), isFile(false) {}

        OutputSegment(int fd, off_t size)
            : offset(0), fileFd(fd), fileRemaining(size), isFile(true) {}
    };

    struct ClientOutput
    {
        std::deque<OutputSegment> segments;
        bool closeWhenDrained;

        ClientOutput() : closeWhenDrained(false) {}
    };

    std::map<int, ClientOutput> clientOutputs;

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
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "";
    }
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
        if (segment.isFile && segment.offset == segment.bytes.size()
            && segment.fileRemaining > 0)
        {
            char buffer[16384];
            size_t amount = segment.fileRemaining > static_cast<off_t>(sizeof(buffer))
                ? sizeof(buffer) : static_cast<size_t>(segment.fileRemaining);
            ssize_t bytesRead = read(segment.fileFd, buffer, amount);
            if (bytesRead <= 0)
                return false;
            segment.bytes.assign(buffer, static_cast<size_t>(bytesRead));
            segment.offset = 0;
            segment.fileRemaining -= bytesRead;
        }

        if (segment.offset < segment.bytes.size())
        {
            ssize_t sent = send(clientFd, segment.bytes.data() + segment.offset,
                                segment.bytes.size() - segment.offset, 0);
            if (sent <= 0)
                return sent < 0;
            segment.offset += static_cast<size_t>(sent);
            if (segment.offset < segment.bytes.size())
                return true;
        }

        if (!segment.isFile || segment.fileRemaining == 0)
        {
            if (segment.isFile && segment.fileFd != -1)
                close(segment.fileFd);
            output->second.segments.pop_front();
        }
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
    std::string baseRoot = root.empty() ? "." : root;
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

bool queueCgiResponse(int clientFd, const CgiJob& job, const CGIResult& result)
{
    if (result.timed_out || result.io_error || result.exit_status != 0)
        return sendErrorResponse(clientFd, job.httpVersion, job.keepAlive, 500);

    CgiResponse parsed;
    if (!parsed.parse(result.raw_output))
        return sendErrorResponse(clientFd, job.httpVersion, job.keepAlive, 500);

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
    response << (job.keepAlive
        ? "Connection: keep-alive\r\n"
        : "Connection: close\r\n");
    response << "\r\n" << parsed.body;
    std::string output = response.str();
    return sendAll(clientFd, output.data(), output.size()) != -1;
}

// bool buildResponse(const HttpParser& http, int clientFd, bool& keepAlive, const std::string& root)
// {
//     std::string version = http.getVersion();

//     std::string connLower = toLower(findHeaderValue(http.getHeaders(), "Connection"));

//     if (!connLower.empty())
//         if (connLower.find("close") != std::string::npos)
//             keepAlive = false;
//         else if (connLower.find("keep-alive") != std::string::npos)
//             keepAlive = true;
//         else
//             keepAlive = (version == "HTTP/1.1");
//     else
//         keepAlive = (version == "HTTP/1.1");

//     if (toLower(http.getMethod()) == "delete")
//         return handleDelete(clientFd, version, keepAlive, http.getTarget(), root);

//     else if (toLower(http.getMethod()) != "get")
//     {
//         std::string resp = makeErrorResponse(405, version, keepAlive, {{"Allow", "GET"}});
//         if (sendAll(clientFd, resp.data(), resp.size()) == -1)
//             return false;
//         return true;
//     }
//     return handleGet(http, version, keepAlive, clientFd, root);
// }

std::string toUpper(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(value[i])));
    return value;
}

bool buildResponse(const HttpParser& http, int clientFd,
                   bool& keepAlive, const ServerConfig& config,
                   CgiHandler& cgi, std::map<int, CgiJob>& cgiJobs)
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

    const LocationConfig* location =
        findMatchingLocation(config, http.getTarget());
    const bool configuredCgi = location != NULL
        && location->path.compare(0, 2, "*.") == 0
        && !location->cgi_path.empty()
        && targetHasExtension(http.getTarget(), location->path.substr(1));

    if (configuredCgi || cgi.isCGI(http.getTarget()))
    {
        if (configuredCgi
            && !isMethodAllowed(config, location, toUpper(http.getMethod())))
            return sendErrorResponse(clientFd, version, keepAlive, 405);

        CGIRequest request;

        std::string target = http.getTarget();
        size_t queryPosition = target.find('?');

        request.method = toUpper(http.getMethod());
        request.script_name = queryPosition == std::string::npos
            ? target
            : target.substr(0, queryPosition);

        const std::string& root = configuredCgi && !location->root.empty()
            ? location->root : config.root;
        request.script_path = resolveRequestedPath(root, request.script_name);
        if (configuredCgi)
            request.interpreter = location->cgi_path;
        request.query_string = queryPosition == std::string::npos
            ? ""
            : target.substr(queryPosition + 1);

        request.body = http.getBody();
        request.headers = http.getHeaders();

        request.server_protocol = version;
        request.server_name = findHeaderValue(http.getHeaders(), "host");
        request.server_software = "webserv/1.0";
        request.server_port = http.getPortNum();

        CgiJob job;
        if (!cgi.start(request, job))
            return sendErrorResponse(clientFd, version, keepAlive, 500);
        job.clientFd = clientFd;
        job.httpVersion = version;
        job.keepAlive = keepAlive;
        cgiJobs[clientFd] = job;
        return true;
    }

    if (toLower(http.getMethod()) == "get")
        return handleGet(http, version, keepAlive, clientFd, config);

    else if (toLower(http.getMethod()) == "post")
        return handlePost(http, version, keepAlive, clientFd, config);

    else if (toLower(http.getMethod()) == "delete")
        return handleDelete(clientFd, version, keepAlive, http.getTarget(), config);

    return false;
}
