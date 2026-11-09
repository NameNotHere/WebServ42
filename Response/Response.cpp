#include "Response.hpp"

std::string Response::statusMsg(int statusCode)
{
    switch (statusCode)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Content Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 505: return "HTTP Version Not Supported";
        default: return "";
    }
}

Response::Response(){}

Response::Response(int, const std::string&){}

Response::Response(const Response&){}

Response& Response::operator=(const Response&)
{
	return *this;
}

Response::~Response(){}
