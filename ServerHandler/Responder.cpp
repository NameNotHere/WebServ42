#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <algorithm>
#include <vector>
#include <map>
#include <utility>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <cctype>
#include "Server.hpp"

std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string statusText(int code)
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

std::string makeErrorResponse(int code, const std::string& version, bool keepAlive, const std::vector<std::pair<std::string,std::string>>& extraHeaders)
{
    std::string body = "<html><body><h1>" + std::to_string(code) + " " + statusText(code) + "</h1></body></html>";
    std::ostringstream out;
    out << version << " " << code << " " << statusText(code) << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Content-Type: text/html\r\n";
    for (size_t i = 0; i < extraHeaders.size(); ++i)
        out << extraHeaders[i].first << ": " << extraHeaders[i].second << "\r\n";
    out << (keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n");
    out << "\r\n";
    out << body;
    return out.str();
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
    size_t total = 0;
    while (total < len)
    {
        ssize_t sent = send(fd, buf + total, len - total, 0);
        if (sent <= 0)
            return -1;
        total += sent;
    }
    return total;
}

static std::string normalizeTarget(const std::string& rawTarget)
{
    std::string target = rawTarget;
    const size_t qpos = target.find('?');

    if (qpos != std::string::npos)
        target = target.substr(0, qpos);
    if (target.empty() || target[0] != '/')
        target = "/" + target;
    return target;
}

static std::string resolveRequestedPath(const std::string& root, const std::string& target)
{
    std::string baseRoot = root.empty() ? "." : root;
    if (!baseRoot.empty() && baseRoot.back() == '/')
        baseRoot.pop_back();

    std::string path = baseRoot + target;
    if (!path.empty() && path.back() == '/')
        path += "index.html";
    return path;
}

static bool sendErrorResponse(int clientFd, const std::string& version, bool keepAlive, int code)
{
    std::string resp = makeErrorResponse(code, version, keepAlive);
    return sendAll(clientFd, resp.data(), resp.size()) != -1;
}

static bool sendFileBody(int clientFd, int fd_in, off_t fileSize)
{
    off_t offset = 0;
    off_t remaining = fileSize;

    while (remaining > 0)
    {
        ssize_t sent = sendfile(clientFd, fd_in, &offset, static_cast<size_t>(remaining));
        if (sent <= 0)
        {
            close(fd_in);
            return false;
        }
        remaining -= sent;
    }

    close(fd_in);
    return true;
}

static bool handleGet(const HttpParser& http, const std::string& version, bool keepAlive, int clientFd, const std::string& root)
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

bool buildResponse(const HttpParser& http, int clientFd, bool& closeConnection, const std::string& root)
{
    std::string version = http.getVersion();

    std::string connLower = toLower(findHeaderValue(http.getHeaders(), "Connection"));
    bool keepAlive;
    if (!connLower.empty())
        if (connLower.find("close") != std::string::npos)
            keepAlive = false;
        else if (connLower.find("keep-alive") != std::string::npos)
            keepAlive = true;
        else
            keepAlive = (version == "HTTP/1.1");
    else
        keepAlive = (version == "HTTP/1.1");
    closeConnection = !keepAlive;

    if (toLower(http.getMethod()) != "get")
    {
        std::string resp = makeErrorResponse(405, version, keepAlive, {{"Allow", "GET"}});
        if (sendAll(clientFd, resp.data(), resp.size()) == -1)
            return false;
        return true;
    }
    return handleGet(http, version, keepAlive, clientFd, root);
}