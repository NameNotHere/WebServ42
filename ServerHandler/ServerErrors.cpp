#include "Server.hpp"


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

bool sendErrorResponse(int clientFd, const std::string& version, bool keepAlive, int code)
{
    std::string resp = makeErrorResponse(code, version, keepAlive);
    return sendAll(clientFd, resp.data(), resp.size()) != -1;
}