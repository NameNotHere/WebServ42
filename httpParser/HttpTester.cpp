#include "HttpParser.hpp"

#include <iostream>

int main(void)
{
	HttpParser test;

	// test.parseHttpRequest("GET /index.html HTTP/1.1\r\nHoSt: loCal-hOst:8080  \r\nConnection:    kEeP-alive \r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)  \r\nContent-length:  42  	\r\nTrAnsFer-encoDing:  chunked		\r\nConTeNt-tYpe:		text/html; charset=\"utf-8\"; 		boundary=\"abc123\"; 		 charset=utf-10	\r\nExpect: 	100-continue	\r\n\r\n");
	test.parseHttpRequest("GET /index.html HTTP/1.1\r\nHoSt: loCal-hOst:8080  \r\nConTeNt-tYpe:		text/html; charset=\"utf\\-8\"; boundary=\"abc123\"; charset=utf-10	\r\n\r\n");
	
	// std::cout << "\n-----Additional tests------\n";
	// std::cout << "Method:" << test.getMethod() << "\n";
	// std::cout << "Target:" << test.getTarget() << "\n";
	// std::cout << "Version:" << test.getVersion() << "\n";

	// std::cout << "\n----Printing Headers----\n";
	// for (std::map<std::string, std::string>::const_iterator it = test.getHeaders().begin();
	// 	it != test.getHeaders().end(); ++it)
	// {
	// 	std::cout << "Name:" << it->first << "\n";
	// 	std::cout << "Value:" << it->second << "\n";
	// }
	return 0;
}