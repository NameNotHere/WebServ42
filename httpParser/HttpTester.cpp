#include "HttpParser.hpp"

#include <iostream>

int main(void)
{
	HttpParser test;

	test.parseHttpRequest("GET /index.html HTTP/1.1\r\n");

	return 0;
}