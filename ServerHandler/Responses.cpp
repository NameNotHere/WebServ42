#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include "Server.hpp"

std::string buildResponse(const std::string& statusLine, const std::string& body, const std::string& contentType)
{
    std::string response =
        "HTTP/1.1 " + statusLine + "\r\n"
        "Content-Type: " + contentType + "\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n" + body;
    return response;
}

void routeRequest(int fd, const HttpParser& http)
{
    const std::string method = http.getMethod();
    const std::string target = http.getTarget();

    std::string response;

    if (method == "GET")
    {
        if (target == "/" || target == "/index.html")
            response = buildResponse("200 OK", "<html><body><h1>webserv42 GET OK</h1></body></html>");
        else
            response = buildResponse("404 Not Found", "<html><body><h1>404 Not Found</h1></body></html>");
    }
    else if (method == "POST")
    {
        if (target == "/upload")
            response = buildResponse("200 OK", "<html><body><h1>POST accepted</h1></body></html>");
        else
            response = buildResponse("404 Not Found", "<html><body><h1>404 Not Found</h1></body></html>");
    }
    else if (method == "DELETE")
    {
        response = buildResponse("200 OK", "<html><body><h1>DELETE accepted</h1></body></html>");
    }
    else
    {
        response = buildResponse("405 Method Not Allowed",
                                 "<html><body><h1>405 Method Not Allowed</h1></body></html>");
    }

    send(fd, response.c_str(), response.size(), 0);
}

#endif
