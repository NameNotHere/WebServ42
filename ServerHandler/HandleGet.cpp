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

bool handleGet(const HttpParser& http, const std::string& version, bool keepAlive, int clientFd, const std::string& root)
{
    std::string target = normalizeTarget(http.getTarget());
    if (target.find("..") != std::string::npos)
    {
        sendErrorResponse(clientFd, version, keepAlive, 400);
        return false;
    }
    const std::string path = resolveRequestedPath(root, target);

    struct stat st;
    if (stat(path.c_str(), &st) == -1 || !S_ISREG(st.st_mode))
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
