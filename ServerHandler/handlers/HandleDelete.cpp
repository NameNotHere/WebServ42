#include "../core/Server.hpp"
#include <cstdio>

bool handleDelete(int clientFd, const std::string& version, bool keepAlive, const std::string& rawTarget, const ServerConfig& config)
{
    const std::string target = normalizeTarget(rawTarget);
    std::string decodedTarget;

    if (!decodeUriPath(target, decodedTarget))
        return sendErrorResponse(clientFd, version, keepAlive, 400, &config);

    if (decodedTarget.find("..") != std::string::npos)
    {
        sendErrorResponse(clientFd, version, keepAlive, 400, &config);
        return false;
    }

    const LocationConfig* location = findMatchingLocation(config, decodedTarget, "DELETE");
    if (!isMethodAllowed(config, location, "DELETE"))
    {
        sendErrorResponse(
            clientFd, version, keepAlive, 405, &config,
            std::vector<std::pair<std::string, std::string> >(1,
                std::make_pair("Allow", allowHeaderValue(config, location))));
        return false;
    }

    const std::string* root = &config.root;
    std::string relativeTarget = decodedTarget;
    if (location != NULL)
    {
        if (!location->root.empty())
            root = &location->root;
        if (location->path.compare(0, 2, "*.") != 0
            && relativeTarget.compare(0, location->path.size(),
                                      location->path) == 0)
        {
            relativeTarget = relativeTarget.substr(location->path.size());
            if (relativeTarget.empty())
                relativeTarget = "/";
            else if (relativeTarget[0] != '/')
                relativeTarget.insert(0, "/");
        }
    }

    std::string filePath = *root;
    if (filePath.empty())
        filePath = ".";
    if (filePath[filePath.size() - 1] == '/')
        filePath.erase(filePath.size() - 1);
    filePath += relativeTarget;
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) != 0)
    {
        sendErrorResponse(clientFd, version, keepAlive, 404, &config);
        return false;
    }
    if (S_ISDIR(fileStat.st_mode))
    {
        sendErrorResponse(clientFd, version, keepAlive, 403, &config);
        return false;
    }
    if (access(filePath.c_str(), W_OK) != 0)
    {
        sendErrorResponse(clientFd, version, keepAlive, 403, &config);
        return false;
    }
    if (std::remove(filePath.c_str()) == 0)
    {
        std::ostringstream out;
        out << version << " 204 " << statusText(204) << "\r\n";
        out << "Content-Length: 0\r\n";
        if (keepAlive)
            out << "Connection: keep-alive\r\n";
        else
            out << "Connection: close\r\n";
        out << "\r\n";
        std::string resp = out.str();
        sendAll(clientFd, resp.data(), resp.size());
        return true;
    }
    else
    {
        sendErrorResponse(clientFd, version, keepAlive, 500, &config);
        return false;
    }
}

bool isMethodAllowed(const ServerConfig& config, const LocationConfig* location, const std::string& method)
{
    // If this location defines allowed_methods, use those.
    if (location != NULL && !location->allowed_methods.empty())
    {
        for (size_t i = 0; i < location->allowed_methods.size(); ++i)
        {
            if (location->allowed_methods[i] == method)
                return true;
        }
        return false;
    }

    // Otherwise, use server-level permissions if configured.
    if (!config.allowed_methods.empty())
    {
        for (size_t i = 0; i < config.allowed_methods.size(); ++i)
        {
            if ((method == "GET" && config.allowed_methods[i] == GET) ||
                (method == "POST" && config.allowed_methods[i] == POST) ||
                (method == "DELETE" && config.allowed_methods[i] == DELETE))
                return true;
        }
        return false;
    }
    return method == "GET";
}

bool decodeUriPath(const std::string& encoded, std::string& decoded)
{
    decoded.clear();

    for (size_t i = 0; i < encoded.size(); ++i)
    {
        if (encoded[i] != '%')
        {
            decoded += encoded[i];
            continue;
        }

        if (i + 2 >= encoded.size())
            return false;

        char hex[3] = { encoded[i + 1], encoded[i + 2], '\0' };
        char* end = NULL;
        long value = std::strtol(hex, &end, 16);

        if (end == hex || *end != '\0')
            return false;

        if (value == 0 || value == '/' || value == '\\')
            return false;

        decoded += static_cast<char>(value);
        i += 2;
    }
    return true;
}
