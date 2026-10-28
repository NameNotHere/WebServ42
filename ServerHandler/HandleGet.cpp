#include "Server.hpp"

std::string GetEnding(const std::string& path)
{
    size_t dot = path.rfind('.');
    if (dot == std::string::npos)
        return "application/octet-stream";
    std::string ext = toLower(path.substr(dot));
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".txt") return "text/plain";
    return "application/octet-stream";
}

bool handleGet(const HttpParser& http, const std::string& version, bool keepAlive, int clientFd, const ServerConfig& config)
{
    std::string target = normalizeTarget(http.getTarget());
    std::string decodedTarget;

    if (!decodeUriPath(target, decodedTarget))
        return sendErrorResponse(clientFd, version, keepAlive, 400);

    const LocationConfig* location =
        findMatchingLocation(config, decodedTarget);
    bool redirectToSlash = false;

    if (!decodedTarget.empty()
        && decodedTarget[decodedTarget.size() - 1] != '/')
    {
        const std::string slashTarget = decodedTarget + "/";
        const LocationConfig* slashLocation =
            findMatchingLocation(config, slashTarget);
        if (slashLocation != NULL
            && slashLocation->path == slashTarget
            && (location == NULL
                || slashLocation->path.size() > location->path.size()))
        {
            location = slashLocation;
            redirectToSlash = true;
        }
    }

    if (!isMethodAllowed(config, location, "GET"))
    {
        sendErrorResponse(clientFd, version, keepAlive, 405);
        return false;
    }
    if (decodedTarget.find("..") != std::string::npos)
    {
        sendErrorResponse(clientFd, version, keepAlive, 400);
        return false;
    }
    if (redirectToSlash)
    {
        std::ostringstream response;
        response << version << " 301 Moved Permanently\r\n"
                 << "Location: " << decodedTarget << "/\r\n"
                 << "Content-Length: 0\r\n"
                 << "Connection: close\r\n\r\n";
        const std::string message = response.str();
        sendAll(clientFd, message.data(), message.size());
        return false;
    }
    const std::string& root =
        (location != NULL && !location->root.empty())
            ? location->root
            : config.root;

    std::string relativeTarget = decodedTarget;
    if (location != NULL && location->path.compare(0, 2, "*.") != 0)
    {
        relativeTarget = decodedTarget.substr(location->path.size());
        if (relativeTarget.empty())
            relativeTarget = "/";
        else if (relativeTarget[0] != '/')
            relativeTarget.insert(0, "/");
    }

    std::string path = root.empty() ? "." : root;
    if (!path.empty() && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);
    path += relativeTarget;
    if (!path.empty() && path[path.size() - 1] == '/')
        path += (location != NULL && !location->index.empty())
            ? location->index
            : "index.html";
    struct stat st;
    if (stat(path.c_str(), &st) == -1)
    {
        sendErrorResponse(clientFd, version, keepAlive, 404);
        return false;
    }
    if (S_ISDIR(st.st_mode) && !decodedTarget.empty()
        && decodedTarget[decodedTarget.size() - 1] != '/')
    {
        std::ostringstream response;
        response << version << " 301 Moved Permanently\r\n"
                 << "Location: " << decodedTarget << "/\r\n"
                 << "Content-Length: 0\r\n"
                 << "Connection: close\r\n\r\n";
        const std::string message = response.str();
        sendAll(clientFd, message.data(), message.size());
        return false;
    }
    if (!S_ISREG(st.st_mode))
    {
        sendErrorResponse(clientFd, version, keepAlive, 404);
        return false;
    }

    const int fd_in = open(path.c_str(), O_RDONLY);
    if (fd_in == -1)
    {
        sendErrorResponse(clientFd, version, keepAlive, 500);
        return std::cout << "Cant open fd\n", false;
    }

    std::ostringstream hdr;
    hdr << version << " 200 " << statusText(200) << "\r\n";
    hdr << "Content-Length: " << st.st_size << "\r\n";
    hdr << "Content-Type: " << GetEnding(path) << "\r\n";
    hdr << (keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n");
    hdr << "\r\n";

    const std::string header = hdr.str();
    if (sendAll(clientFd, header.data(), header.size()) == -1)
    {
        close(fd_in);
        return false;
    }
    return sendFileBody(clientFd, fd_in, st.st_size);
}
