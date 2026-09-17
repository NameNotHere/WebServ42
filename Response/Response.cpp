#include "Response.hpp"

std::string Response::statusMsg(int statusCode)
{
	if (statusCode == 200)
        return "OK";
    if (statusCode == 400)
        return "Bad Request";
    if (statusCode == 404)
        return "Not Found";
    if (statusCode == 405)
        return "Method Not Allowed";
    if (statusCode == 500)
        return "Internal Server Error";

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

// Response::Response() : statusCode_(200), body_(){}

// Response::Response(int statusCode, const std::string& body)
// 	: statusCode_(statusCode), body_(body){}

// Response::Response(const Response& other)
// 	: statusCode_(other.statusCode_), body_(other.body_){}

// Response& Response::operator=(const Response& other)
// {
// 	if (this != &other)
// 	{
// 		statusCode_ = other.statusCode_;
// 		body_ = other.body_;
// 	}
// 	return *this;
// }

Response::~Response(){}