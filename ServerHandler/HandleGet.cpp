#include "Server.hpp"

std::string GetEnding(const std::string& path);

namespace
{
    const off_t SMALL_FILE_LIMIT = 64 * 1024;

    struct CachedStaticFile
    {
        off_t size;
        time_t modified;
        std::string body;
        std::string headerPrefix;
    };

    std::map<std::string, CachedStaticFile> smallFileCache;

    bool readSmallFile(const std::string& path, const struct stat& info,
                       const std::string& version, std::string& header,
                       std::string& body)
    {
        std::map<std::string, CachedStaticFile>::iterator cached =
            smallFileCache.find(path);
        if (cached == smallFileCache.end() || cached->second.size != info.st_size
            || cached->second.modified != info.st_mtime)
        {
            int fd = open(path.c_str(), O_RDONLY);
            if (fd == -1)
                return false;

            CachedStaticFile entry;
            entry.size = info.st_size;
            entry.modified = info.st_mtime;
            entry.body.resize(static_cast<size_t>(info.st_size));
            size_t offset = 0;
            while (offset < entry.body.size())
            {
                ssize_t count = read(fd, &entry.body[offset], entry.body.size() - offset);
                if (count <= 0)
                {
                    close(fd);
                    return false;
                }
                offset += static_cast<size_t>(count);
            }
            close(fd);

            std::ostringstream prefix;
            prefix << " 200 OK\r\nContent-Length: " << info.st_size
                   << "\r\nContent-Type: " << GetEnding(path) << "\r\n";
            entry.headerPrefix = prefix.str();
            cached = smallFileCache.insert_or_assign(path, std::move(entry)).first;
        }
        header = version + cached->second.headerPrefix;
        body = cached->second.body;
        return true;
    }

    const LocationConfig* selectGetLocation(
        const ServerConfig& config, const std::string& decodedTarget,
        bool& redirectToSlash)
    {
        const LocationConfig* location =
            findMatchingLocation(config, decodedTarget, "GET");
        redirectToSlash = false;

        if (!decodedTarget.empty()
            && decodedTarget[decodedTarget.size() - 1] != '/')
        {
            const std::string slashTarget = decodedTarget + "/";
            const LocationConfig* slashLocation =
                findMatchingLocation(config, slashTarget, "GET");
            if (slashLocation != NULL
                && slashLocation->path == slashTarget
                && (location == NULL
                    || slashLocation->path.size() > location->path.size()))
            {
                redirectToSlash = true;
                location = slashLocation;
            }
        }
        return location;
    }

    bool sendDirectoryRedirect(int clientFd, const std::string& version,
                               const std::string& decodedTarget)
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

    std::string resolveGetPath(const std::string& decodedTarget,
                               const LocationConfig* location,
                               const ServerConfig& config)
    {
        const std::string* root = &config.root;
        if (location != NULL && !location->root.empty())
            root = &location->root;

        std::string relativeTarget = decodedTarget;
        if (location != NULL && location->path.compare(0, 2, "*.") != 0)
        {
            relativeTarget = decodedTarget.substr(location->path.size());
            if (relativeTarget.empty())
                relativeTarget = "/";
            else if (relativeTarget[0] != '/')
                relativeTarget.insert(0, "/");
        }

        std::string path = *root;
        if (path.empty())
            path = ".";
        if (!path.empty() && path[path.size() - 1] == '/')
            path.erase(path.size() - 1);
        path += relativeTarget;
        return path;
    }

    bool sendSmallFile(int clientFd, const std::string& path,
                       const struct stat& info, const std::string& version,
                       bool keepAlive, const ServerConfig& config)
    {
        std::string header;
        std::string body;
        if (!readSmallFile(path, info, version, header, body))
        {
            sendErrorResponse(clientFd, version, keepAlive, 500, &config);
            return false;
        }
        if (keepAlive)
            header += "Connection: keep-alive\r\n\r\n";
        else
            header += "Connection: close\r\n\r\n";
        sendAll(clientFd, header.data(), header.size());
        sendAll(clientFd, body.data(), body.size());
        return true;
    }

    bool sendLargeFile(int clientFd, const std::string& path,
                       const struct stat& info, const std::string& version,
                       bool keepAlive, const ServerConfig& config)
    {
        const int fileFd = open(path.c_str(), O_RDONLY);
        if (fileFd == -1)
        {
            sendErrorResponse(clientFd, version, keepAlive, 500, &config);
            return std::cout << "Cant open fd\n", false;
        }

        std::ostringstream response;
        response << version << " 200 " << statusText(200) << "\r\n";
        response << "Content-Length: " << info.st_size << "\r\n";
        response << "Content-Type: " << GetEnding(path) << "\r\n";
        if (keepAlive)
            response << "Connection: keep-alive\r\n";
        else
            response << "Connection: close\r\n";
        response << "\r\n";

        const std::string header = response.str();
        if (sendAll(clientFd, header.data(), header.size()) == -1)
        {
            close(fileFd);
            return false;
        }
        return sendFileBody(clientFd, fileFd, info.st_size);
    }

    bool serveGetFile(int clientFd, const std::string& path,
                      const std::string& decodedTarget,
                      const std::string& version, bool keepAlive,
                      const ServerConfig& config)
    {
        struct stat info;
        if (stat(path.c_str(), &info) == -1)
        {
            sendErrorResponse(clientFd, version, keepAlive, 404, &config);
            return false;
        }
        if (S_ISDIR(info.st_mode) && !decodedTarget.empty()
            && decodedTarget[decodedTarget.size() - 1] != '/')
            return sendDirectoryRedirect(clientFd, version, decodedTarget);
        if (!S_ISREG(info.st_mode))
        {
            sendErrorResponse(clientFd, version, keepAlive, 404, &config);
            return false;
        }
        if (info.st_size <= SMALL_FILE_LIMIT)
            return sendSmallFile(clientFd, path, info, version, keepAlive,
                                 config);
        return sendLargeFile(clientFd, path, info, version, keepAlive,
                             config);
    }
}

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

    bool sendDirectoryListing(int clientFd, const std::string& dirPath,
                              const std::string& urlPath,
                              const std::string& version, bool keepAlive,
                              const ServerConfig& config)
    {
        DIR* dir = opendir(dirPath.c_str());
        if (dir == NULL)
            return sendErrorResponse(clientFd, version, keepAlive, 403, &config);

        std::string body = "<html><body><h1>Index of " + urlPath + "</h1><ul>";

        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL)
        {
            std::string name = entry->d_name;
            if (name == "." || name == "..")
                continue;
            body += "<li><a href=\"" + urlPath + name + "\">" + name + "</a></li>";
        }
        closedir(dir);
        body += "</ul></body></html>";

        // Send headers + page
        std::ostringstream response;
        response << version << " 200 OK\r\n";
        response << "Content-Type: text/html\r\n";
        response << "Content-Length: " << body.size() << "\r\n";
        if (keepAlive)
            response << "Connection: keep-alive\r\n";
        else
            response << "Connection: close\r\n";
        response << "\r\n";
        response << body;

        std::string out = response.str();
        sendAll(clientFd, out.data(), out.size());
        return true;
    }

bool handleGet(const HttpParser& http, const std::string& version, bool keepAlive, int clientFd, const ServerConfig& config)
{
    std::string target = normalizeTarget(http.getTarget());
    std::string decodedTarget;

    if (!decodeUriPath(target, decodedTarget))
        return sendErrorResponse(clientFd, version, keepAlive, 400, &config);

    bool redirectToSlash = false;
    const LocationConfig* location =
        selectGetLocation(config, decodedTarget, redirectToSlash);

    if (!isMethodAllowed(config, location, "GET"))
    {
        sendErrorResponse(
            clientFd, version, keepAlive, 405, &config,
            std::vector<std::pair<std::string, std::string> >(1,
                std::make_pair("Allow", allowHeaderValue(config, location))));
        return false;
    }
    if (decodedTarget.find("..") != std::string::npos)
    {
        sendErrorResponse(clientFd, version, keepAlive, 400, &config);
        return false;
    }
    if (redirectToSlash)
        return sendDirectoryRedirect(clientFd, version, decodedTarget);
    std::string path = resolveGetPath(decodedTarget, location, config);
    if (!path.empty() && path[path.size() - 1] == '/')
    {
        std::string index = "index.html";
        if (location != NULL && !location->index.empty())
            index = location->index;

        struct stat info;
        if (stat((path + index).c_str(), &info) == 0)
            path += index;
        else if (location != NULL && location->autoindex)
            return sendDirectoryListing(clientFd, path, decodedTarget,
                                        version, keepAlive, config);
        else
            path += index;
    }
    return serveGetFile(clientFd, path, decodedTarget, version, keepAlive,
                        config);
}
