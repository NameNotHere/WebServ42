#include "HttpParser.hpp"

void HttpParser::parseHttpRequest(const std::string& request)
{
	if(!requestLine(request))
	{
		std::cout << "Invalid HTTP Request!\n";
		exit(1);
	}
	else
		std::cout << "Valid HTTP Request!\n";

	if(!headers(request))
	{
		std::cout << "Invalid Header\n";
		exit(1);
	}
	else
		std::cout << "Valid Header\n";
	if(!body(request))
	{
		std::cout << "Invalid Body\n";
		exit(1);
	}
	else
		std::cout << "Valid Body\n";
}

const std::string& HttpParser::getMethod() const
{
	return _method;
}

const std::string& HttpParser::getTarget() const
{
	return _target;
}

const std::string& HttpParser::getVersion() const
{
	return _version;
}

const std::map<std::string, std::string>& HttpParser::getHeaders() const
{
    return _headers;
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