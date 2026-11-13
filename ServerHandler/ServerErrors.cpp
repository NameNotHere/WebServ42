#include "Server.hpp"

std::string makeErrorResponse(int code, const std::string& version, bool keepAlive, const std::vector<std::pair<std::string,std::string>>& extraHeaders, const ServerConfig* config)
{
    std::string body = "<html><body><h1>" + std::to_string(code) + " " + statusText(code) + "</h1></body></html>";
    if (config != NULL)
    {
        std::map<int, std::string>::const_iterator pagePath =
            config->errors.find(code);
        if (pagePath != config->errors.end())
        {
            std::ifstream page(pagePath->second.c_str(),
                               std::ios::in | std::ios::binary);
            if (page.is_open())
            {
                std::ostringstream contents;
                contents << page.rdbuf();
                if (!page.bad())
                    body = contents.str();
            }
        }
    }
    std::ostringstream out;
    out << version << " " << code << " " << statusText(code) << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Content-Type: text/html\r\n";
    for (size_t i = 0; i < extraHeaders.size(); ++i)
        out << extraHeaders[i].first << ": " << extraHeaders[i].second << "\r\n";
    if (keepAlive)
        out << "Connection: keep-alive\r\n";
    else
        out << "Connection: close\r\n";
    out << "\r\n";
    out << body;
    return out.str();
}

bool sendErrorResponse(
    int clientFd, const std::string& version, bool keepAlive, int code,
    const ServerConfig* config,
    const std::vector<std::pair<std::string, std::string> >& extraHeaders)
{
    std::string resp = makeErrorResponse(code, version, keepAlive,
                                         extraHeaders,
                                         config);
    return sendAll(clientFd, resp.data(), resp.size()) != -1;
}
