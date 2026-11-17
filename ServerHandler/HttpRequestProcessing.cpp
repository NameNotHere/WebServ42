#include "Server.hpp"
#include "ServerInternal.hpp"

using namespace ServerInternal;

namespace
{
    bool isLargePost(const HttpParser& http)
    {
        if (http.getMethod() != "POST")
            return false;
        const std::map<std::string, std::string>& headers = http.getHeaders();
        if (headers.find("transfer-encoding") != headers.end())
            return true;
        std::map<std::string, std::string>::const_iterator it = headers.find("content-length");
        if (it == headers.end())
            return false;
        try { return std::stoull(it->second) > LARGE_POST_THRESHOLD; }
        catch (const std::exception&) { return false; }
    }
}

namespace
{
    void rejectBareLfRequest(int fd, std::map<int, std::string>& reqs, std::map<int, ServerConfig>& clientConfigs)
    {
        const std::string& request = reqs[fd];
        for (std::string::size_type i = 0; i < request.size(); ++i)
        {
            if (request[i] == '\n'
                && (i == 0 || request[i - 1] != '\r'))
            {
                std::map<int, ServerConfig>::iterator config =
                    clientConfigs.find(fd);
                const ServerConfig* serverConfig = NULL;
                if (config != clientConfigs.end())
                    serverConfig = &config->second;
                sendErrorResponse(fd, "HTTP/1.1", false, 400, serverConfig);
                closeAfterPendingOutput(fd);
                reqs.erase(fd);
                ServerInternal::completeClientRequest(fd, false);
                clientConfigs.erase(fd);
                largePostClients.erase(fd);
                largePostWaiters.erase(fd);
                return;
            }
        }
    }

    size_t requestMaxBody(int fd, const std::string& rawRequest, std::map<int, ServerConfig>& clientConfigs)
    {
        size_t maxBody = DEFAULT_MAX_REQUEST_BODY;
        size_t firstSpace = rawRequest.find(' ');
        size_t secondSpace = std::string::npos;
        if (firstSpace != std::string::npos)
            secondSpace = rawRequest.find(' ', firstSpace + 1);

        if (firstSpace != std::string::npos
            && secondSpace != std::string::npos)
        {
            std::string method = rawRequest.substr(0, firstSpace);
            std::string target = rawRequest.substr(
                firstSpace + 1, secondSpace - firstSpace - 1);

            const LocationConfig* location = findMatchingLocation(clientConfigs.at(fd), target, method);

            if (location != NULL && location->hasMaxBody)
                maxBody = location->maxBody;
        }
        return maxBody;
    }

    bool processValidRequest(int fd, HttpParser& http, bool& keepAlive,
                             std::map<int, std::string>& reqs,
                             std::map<int, ServerConfig>& clientConfigs, CgiHandler& cgi,
                             std::map<int, CgiJob>& cgiJobs)
    {
        size_t requestLen = http.getRequestLength();
        bool responseBuilt = buildResponse(http, fd, keepAlive, clientConfigs.at(fd), cgi, cgiJobs);
        reqs[fd].erase(0, requestLen);
        ServerInternal::completeClientRequest(fd, !reqs[fd].empty());
        if (cgiJobs.find(fd) == cgiJobs.end())
            largePostClients.erase(fd);

        largePostWaiters.erase(fd);
        if (cgiJobs.find(fd) != cgiJobs.end())
        {
            if (reqs[fd].empty())
                std::string().swap(reqs[fd]);
            return false;
        }
        if (!responseBuilt)
        {
            closeAfterPendingOutput(fd);
            reqs.erase(fd);
            ServerInternal::completeClientRequest(fd, false);
            clientConfigs.erase(fd);
            return false;
        }
        if (!keepAlive)
        {
            closeAfterPendingOutput(fd);
            reqs.erase(fd);
            clientConfigs.erase(fd);
            return false;
        }
        return !reqs[fd].empty();
    }

    void sendParserErrorResponse(int fd, HttpParser::RequestStatus status,
                                 std::map<int, std::string>& reqs,
                                 std::map<int, ServerConfig>& clientConfigs)
    {
        int code = 400;
        if (status == HttpParser::REQUEST_METHOD_NOT_ALLOWED) code = 405;
        else if (status == HttpParser::REQUEST_METHOD_NOT_IMPLEMENTED) code = 501;
        else if (status == HttpParser::REQUEST_TARGET_NOT_FOUND) code = 404;
        else if (status == HttpParser::REQUEST_VERSION_NOT_SUPPORTED) code = 505;
        else if (status == HttpParser::REQUEST_CONTENT_TOO_LARGE) code = 413;

        std::map<int, ServerConfig>::iterator config = clientConfigs.find(fd);
        std::vector<std::pair<std::string, std::string> > extraHeaders;

        if (code == 405 && config != clientConfigs.end())
        {
            const LocationConfig* location = NULL;
            std::map<int, std::string>::const_iterator request = reqs.find(fd);
            if (request != reqs.end())
            {
                size_t firstSpace = request->second.find(' ');
                if (firstSpace != std::string::npos)
                {
                    size_t secondSpace = request->second.find(' ', firstSpace + 1);
                    if (secondSpace != std::string::npos)
                    {
                        std::string target = request->second.substr(
                            firstSpace + 1, secondSpace - firstSpace - 1);
                        location = findMatchingLocation(
                            config->second, target, "HEAD");
                    }
                }
            }
            extraHeaders.push_back(std::make_pair("Allow", allowHeaderValue(config->second, location)));
        }
        const ServerConfig* serverConfig = NULL;
        if (config != clientConfigs.end())
            serverConfig = &config->second;
        sendErrorResponse(fd, "HTTP/1.1", false, code, serverConfig, extraHeaders);
        closeAfterPendingOutput(fd);
        reqs.erase(fd);
        ServerInternal::completeClientRequest(fd, false);
        clientConfigs.erase(fd);
        largePostClients.erase(fd);
        largePostWaiters.erase(fd);
    }
}

void runHttpParser(int fd, std::map<int, std::string>& reqs,
                   std::map<int, ServerConfig>& clientConfigs, CgiHandler& cgi,
                   std::map<int, CgiJob>& cgiJobs)
{
    bool keepAlive = true;
    HttpParser http;
    while (true)
    {
        if (reqs[fd].find("\r\n\r\n") == std::string::npos)
        {
            rejectBareLfRequest(fd, reqs, clientConfigs);
            return;
        }

        size_t maxBody = requestMaxBody(fd, reqs[fd], clientConfigs);
        HttpParser::RequestStatus status = http.parseHttpRequest(reqs[fd], maxBody);
        if (status == HttpParser::REQUEST_VALID)
        {
            if (!processValidRequest(fd, http, keepAlive, reqs, clientConfigs, cgi, cgiJobs))
                return;
            continue;
        }
        else if (status == HttpParser::REQUEST_INCOMPLETE)
        {
            if (isLargePost(http))
            {
                if (largePostClients.count(fd))
                    return;
                if (largePostClients.size() < MAX_BUFFERED_LARGE_POSTS)
                    largePostClients.insert(fd);
                else
                    largePostWaiters.insert(fd);
            }
            return;
        }
        else
        {
            sendParserErrorResponse(fd, status, reqs, clientConfigs);
            largePostClients.erase(fd);
            largePostWaiters.erase(fd);
            return;
        }
    }
}
