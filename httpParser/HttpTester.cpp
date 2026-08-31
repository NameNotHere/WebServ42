#include "HttpParser.hpp"

#include <iostream>

int main(void)
{
	HttpParser test;

	test.parseHttpRequest("GET /index.html HTTP/1.1\r\nHoSt: loCalhOst:8080  \r\nConnection:    keep-alive	\r\nUser-Agent: 	Mozilla/5.0		\r\nContent-Length: 42\r\n\r\n");

	return 0;
}