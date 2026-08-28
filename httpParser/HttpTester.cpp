#include "HttpParser.hpp"

#include <iostream>

int main(void)
{
	HttpParser test;

	std::cout << "Valid request:" << std::endl;
	test.parseHttpRequest("GET /index.html HTTP/1.1\r\n");

	std::cout << "Valid method and target:" << std::endl;
	test.parseHttpRequest("POST /submit HTTP/1.1\r\n");

	std::cout << "Invalid HTTP version:" << std::endl;
	test.parseHttpRequest("GET /index.html HTTP/1.0\r\n");

	std::cout << "Missing target:" << std::endl;
	test.parseHttpRequest("GET HTTP/1.1\r\n");

	std::cout << "Target does not start with '/':" << std::endl;
	test.parseHttpRequest("GET index.html HTTP/1.1\r\n");

	std::cout << "Missing HTTP version:" << std::endl;
	test.parseHttpRequest("GET /index.html\r\n");

	std::cout << "Invalid character in method:" << std::endl;
	test.parseHttpRequest("GE@T /index.html HTTP/1.1\r\n");

	std::cout << "Missing CRLF:" << std::endl;
	test.parseHttpRequest("GET /index.html HTTP/1.1");

	return 0;
}