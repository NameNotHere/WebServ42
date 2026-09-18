#include "Server.hpp"

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

std::string normalizeTarget(const std::string& rawTarget)
{
    std::string target = rawTarget;
    const size_t qpos = target.find('?');

    if (qpos != std::string::npos)
        target = target.substr(0, qpos);
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

    if (toLower(http.getMethod()) == "DELETE")
        return handleDelete(clientFd, version, keepAlive, http.getTarget(), root);
    else if (toLower(http.getMethod()) != "get")
    {
        std::string resp = makeErrorResponse(405, version, keepAlive, {{"Allow", "GET"}});
        if (sendAll(clientFd, resp.data(), resp.size()) == -1)
            return false;
        return true;
    }
    return handleGet(http, version, keepAlive, clientFd, root);
}