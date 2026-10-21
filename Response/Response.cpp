#include "Response.hpp"

std::string Response::statusMsg(int statusCode)
{
	if (statusCode == 200)
        return "OK";
    else if (statusCode == 400)
        return "Bad Request";
    else if (statusCode == 404)
        return "Not Found";
    else if (statusCode == 405)
        return "Method Not Allowed";
	else if (statusCode == 413)
    	return "Content Too Large";
    else if (statusCode == 500)
        return "Internal Server Error";
    else if (statusCode == 505)
        return "HTTP Version Not Supported";
    else if (statusCode == 501)
        return "Method not Implemented";

    return "Unknown";
}

std::string Response::create(int statusCode, const std::string& body)
{
	std::string response;

    response += "HTTP/1.1 ";
    response += std::to_string(statusCode);
	response += " ";
	response += statusMsg(statusCode);
    response += "\r\n";

    response += "Content-Length: ";
    response += std::to_string(body.size());
    response += "\r\n";

    response += "Connection: close\r\n";
    response += "\r\n";

    response += body;

    return response;
}

Response::Response(){}

Response::Response(int, const std::string&){}

Response::Response(const Response&){}

Response& Response::operator=(const Response&)
{
	return *this;
}

Response::~Response(){}