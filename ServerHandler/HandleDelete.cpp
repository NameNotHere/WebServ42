#include "Server.hpp"

bool handleDelete(int clientFd, const std::string& version, bool keepAlive, const std::string& rawTarget, const ServerConfig& config)
{
    const std::string target = normalizeTarget(rawTarget);
    std::string decodedTarget;

    if (!decodeUriPath(target, decodedTarget))
        return sendErrorResponse(clientFd, version, keepAlive, 400);

    if (target.find("..") != std::string::npos)
    {
        sendErrorResponse(clientFd, version, keepAlive, 400);
        return false;
    }

    const LocationConfig* location = findMatchingLocation(config, target);
    if (!isMethodAllowed(config, location, "DELETE"))
    {
        sendErrorResponse(clientFd, version, keepAlive, 405);
        return false;
    }

    const std::string filePath = resolveRequestedPath(config.root, target);
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) != 0)
    {
        sendErrorResponse(clientFd, version, keepAlive, 404);
        return false;
    }
    if (S_ISDIR(fileStat.st_mode))
    {
        sendErrorResponse(clientFd, version, keepAlive, 403);
        return false;
    }
    if (access(filePath.c_str(), W_OK) != 0)
    {
        sendErrorResponse(clientFd, version, keepAlive, 403);
        return false;
    }
    if (unlink(filePath.c_str()) == 0)
    {
        std::ostringstream out;
        out << version << " 204 " << statusText(204) << "\r\n";
        out << "Content-Length: 0\r\n";
        out << (keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n");
        out << "\r\n";
        std::string resp = out.str();
        sendAll(clientFd, resp.data(), resp.size());
        return true;
    }
    else
    {
        sendErrorResponse(clientFd, version, keepAlive, 500);
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

    // Without an explicit method list, locations are read-only by default.
    // POST and DELETE must be enabled in the location or server config.
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

        // A percent sign must be followed by two hexadecimal digits.
        if (i + 2 >= encoded.size())
            return false;

        char hex[3] = { encoded[i + 1], encoded[i + 2], '\0' };
        char* end = NULL;
        long value = std::strtol(hex, &end, 16);

        if (end == hex || *end != '\0')
            return false;

        // Reject decoded NUL bytes and encoded path separators
        // to avoid ambiguous path handling.
        if (value == 0 || value == '/' || value == '\\')
            return false;

        decoded += static_cast<char>(value);
        i += 2;
    }

    return true;
}
