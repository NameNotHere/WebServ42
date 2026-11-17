#include "Server.hpp"
#include "Responder.hpp"
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
    int cgiScriptErrorStatus(const std::string& path)
    {
        struct stat info;
        if (stat(path.c_str(), &info) == -1)
        {
            if (errno == EACCES || errno == EPERM)
                return 403;
            if (errno == ENOENT || errno == ENOTDIR)
                return 404;
            return 500;
        }
        if (!S_ISREG(info.st_mode))
            return 404;
        if (access(path.c_str(), R_OK) == -1)
        {
            if (errno == EACCES || errno == EPERM)
                return 403;
            if (errno == ENOENT || errno == ENOTDIR)
                return 404;
            return 500;
        }
        return 0;
    }

    bool targetHasExtension(const std::string& target,
                           const std::string& extension)
    {
        const size_t queryPosition = target.find('?');
        const std::string path = target.substr(0, queryPosition);
        return path.size() >= extension.size()
            && path.compare(path.size() - extension.size(), extension.size(), extension) == 0;
    }
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
                              bool keepAlive,
                              const LocationConfig& location)
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

    bool isConfiguredCgiRoute(const LocationConfig* location, const std::string& target)
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
                scriptPathUri = "/" + scriptUri.substr(lastSlash + 1);
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
        if (configuredCgi && !isMethodAllowed(config, location, toUpper(http.getMethod())))
        {
            std::vector<std::pair<std::string, std::string> > extraHeaders;
            extraHeaders.push_back(std::make_pair("Allow", allowHeaderValue(config, location)));
            return sendErrorResponse(clientFd, version, keepAlive, 405, &config, extraHeaders);
        }
        CGIRequest request = makeCgiRequest(http, config, location, configuredCgi, version);
        int scriptError = cgiScriptErrorStatus(request.script_path);
        if (scriptError != 0)
            return sendErrorResponse(clientFd, version, keepAlive,
                                     scriptError, &config);
        CgiJob job;
        unsigned int timeoutSeconds = 5;

        if (request.body.size() >= 8 * 1024 * 1024)
            timeoutSeconds = 30;
        if (!cgi.start(request, job, timeoutSeconds))
            return sendErrorResponse(clientFd, version, keepAlive, 500, &config);

        job.clientFd = clientFd;
        job.httpVersion = version;
        job.keepAlive = keepAlive;
        cgiJobs.emplace(clientFd, std::move(job));
        return true;
    }

    bool dispatchResponse(HttpParser& http, int clientFd, bool keepAlive, const ServerConfig& config)
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
