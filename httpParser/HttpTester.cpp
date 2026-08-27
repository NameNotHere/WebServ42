#include "HttpParser.hpp"

int main(void)
{
	HttpParser test;
	test.parseHttpRequest("GET/index.html/HTTP 1.1\r\n");
	return 0;
}