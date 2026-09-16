#include "Post.hpp"

std::string Post::handleRequest(const HttpParser& request)
{
	std::cout << "Post target:" << request.getTarget() << "\n";
	std::cout << "Post body:" << request.getBody() << "\n";

	std::string response =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: close\r\n"
		"\r\n"
		"OK";

	return response;
}

Post::Post(){}

Post::Post(const Post& other)
{
	(void) other;
}

Post& Post::operator=(const Post& other)
{
	(void) other;
	// if (this != &other)
	// {

	// }
	return *this;

}

Post::~Post(){}