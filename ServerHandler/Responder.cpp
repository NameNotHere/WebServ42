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
    const size_t hashPos = target.find('#');

    if (qpos != std::string::npos)
        target = target.substr(0, qpos);
    if (hashPos != std::string::npos)
        target = target.substr(0, hashPos);
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

// bool buildResponse(const HttpParser& http, int clientFd, bool& keepAlive, const std::string& root)
// {
//     std::string version = http.getVersion();

//     std::string connLower = toLower(findHeaderValue(http.getHeaders(), "Connection"));

//     if (!connLower.empty())
//         if (connLower.find("close") != std::string::npos)
//             keepAlive = false;
//         else if (connLower.find("keep-alive") != std::string::npos)
//             keepAlive = true;
//         else
//             keepAlive = (version == "HTTP/1.1");
//     else
//         keepAlive = (version == "HTTP/1.1");

//     if (toLower(http.getMethod()) == "delete")
//         return handleDelete(clientFd, version, keepAlive, http.getTarget(), root);

//     else if (toLower(http.getMethod()) != "get")
//     {
//         std::string resp = makeErrorResponse(405, version, keepAlive, {{"Allow", "GET"}});
//         if (sendAll(clientFd, resp.data(), resp.size()) == -1)
//             return false;
//         return true;
//     }
//     return handleGet(http, version, keepAlive, clientFd, root);
// }

std::string toUpper(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(value[i])));
    return value;
}

bool buildResponse(const HttpParser& http, int clientFd,
                   bool& keepAlive, const std::string& root) //new build response, build with AI. Need to change some stuff and make sure it's bullet proof.
{
    std::string version = http.getVersion();

    // Keep-alive calculation remains here.

    CgiHandler cgi("/usr/bin/python3", 5);

    if (cgi.isCGI(http.getTarget()))
    {
        CGIRequest request;

        std::string target = http.getTarget();
        size_t queryPosition = target.find('?');

        request.method = toUpper(http.getMethod());
        request.script_name = queryPosition == std::string::npos
            ? target
            : target.substr(0, queryPosition);

        request.script_path = resolveRequestedPath(root, request.script_name);
        request.query_string = queryPosition == std::string::npos
            ? ""
            : target.substr(queryPosition + 1);

        request.body = http.getBody();
        request.headers = http.getHeaders();

        request.server_protocol = version;
        request.server_name = findHeaderValue(http.getHeaders(), "host");
        request.server_software = "webserv/1.0";
        request.server_port = "8080";

        CGIResult result = cgi.execute(request);

        if (result.timed_out || result.io_error || result.exit_status != 0)
            return sendErrorResponse(clientFd, version, keepAlive, 500);

        CgiResponse parsed;
        if (!parsed.parse(result.raw_output))
            return sendErrorResponse(clientFd, version, keepAlive, 500);

        std::ostringstream response;
        response << version << " "
                 << parsed.status_code << " "
                 << parsed.status_message << "\r\n";

        for (std::map<std::string, std::string>::const_iterator it =
                 parsed.headers.begin();
             it != parsed.headers.end(); ++it)
        {
            if (it->first == "content-length"
                || it->first == "connection")
                continue;

            response << it->first << ": " << it->second << "\r\n";
        }

        response << "Content-Length: " << parsed.body.size() << "\r\n";
        response << (keepAlive
            ? "Connection: keep-alive\r\n"
            : "Connection: close\r\n");
        response << "\r\n";
        response << parsed.body;

        std::string output = response.str();
        return sendAll(clientFd, output.data(), output.size()) != -1;
    }

    //THIS IS WHERE WE CAN INSERT CGI
    //(if isCgi(http.getTarget()))
    //  retrun handleCgi();
    /*else if*/if (toLower(http.getMethod()) == "get")
        return handleGet(http, version, keepAlive, clientFd, root);

    else if (toLower(http.getMethod()) == "post")
    //INSTEAD OF HANDLE GET FOR "POST"
    //WE CALL "HANDLE POST"
        return handleGet(http, version, keepAlive, clientFd, root);

    else if (toLower(http.getMethod()) == "delete")
        return handleDelete(clientFd, version, keepAlive, http.getTarget(), root);
    return false;
}
