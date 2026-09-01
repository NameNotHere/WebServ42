#include "HttpParser.hpp"

#include <iostream>

int main(void)
{
	HttpParser test;

	// test.parseHttpRequest("GET /index.html HTTP/1.1\r\nHoSt: loCalhOst:8080  \r\nConnection:    keep-alive	\r\nUser-Agent: 	Mozilla/5.0		\r\nContent-Length: 42\r\n\r\n");
	test.parseHttpRequest("GET /index.html HTTP/1.1\r\nHoSt: loCal-hOst:8080  \r\nConnection:    kEeP-alive \r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)  \r\nContent-length:  42  	\r\nTrAnsFer-encoDing:  chunked		\r\nConTeNt-tYpe:		text/html; charset=utf-8; 		boundary=abc123; 		 charset=utf-10;\r\n\r\n");
	// test.parseHttpRequest("GET /index.html HTTP/1.1\r\nHoSt: loCal-hOst:8080  \r\nConTeNt-tYpe:		text/html; charset=utf-8; 		boundary=abc123; 		 charset=utf-10;	\r\n\r\n");

	return 0;
}