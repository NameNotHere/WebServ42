#include "HttpParser.hpp"

#include <iostream>

int main(void)
{
	HttpParser test;

	test.parseHttpRequest("GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\n");

	return 0;
}