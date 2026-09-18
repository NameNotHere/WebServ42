#include "Server.hpp"

bool handleDelete(int clientFd, const std::string& version, bool keepAlive, const std::string& rawTarget, const std::string& root)
{
    const std::string target = normalizeTarget(rawTarget);
    if (target.find("..") != std::string::npos)
    {
        sendErrorResponse(clientFd, version, keepAlive, 400);
        return false;
    }
    const std::string filePath = resolveRequestedPath(root, target); // what the fuck

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
