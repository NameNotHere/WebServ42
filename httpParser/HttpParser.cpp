#include "HttpParser.hpp"

void HttpParser::parseHttpRequest(const std::string& request)
{
	if(!requestLine(request))
		std::cout << "Invalid HTTP Request!\n";
	else
		std::cout << "Valid HTTP Request!\n";

	if(!headers(request))
		std::cout << "Invalid Header\n";
	else
		std::cout << "Valid Header\n";
}

HttpParser::HttpParser(){}

HttpParser::HttpParser(const HttpParser& other)
	: _method(other._method), _target(other._target), _version(other._version){}

HttpParser& HttpParser::operator=(const HttpParser& other)
{
	if (this != &other)
	{
		_method = other._method;
		_target = other._target;
		_version = other._version;
	}
	return *this;
}

HttpParser::~HttpParser(){}