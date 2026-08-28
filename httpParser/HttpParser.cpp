#include "HttpParser.hpp"

//Need to do char check for targetAndVersion
//Fix || statement

void HttpParser::parseHttpRequest(const std::string& request)
{
	if(!checkMethod(request) || !checkTargetAndVersion(request))
		std::cout << "Invalid HTTP Request!\n";
	else
		std::cout << "Valid HTTP Request!\n";
}

bool HttpParser::checkMethod(const std::string& request)
{
	size_t pos = request.find(' ');

	if(pos == std::string::npos || request[pos + 1] != '/')
		return false;

	std::string method = request.substr(0, pos);
	// std::cout << method << "\n";
	
	for(size_t i = 0; i < method.size(); i++)
	{
		if(!isalnum(method[i]) && method[i] != '-' && method[i] != '!' && method[i] != '#'
		   && method[i] != '$' && method[i] != '%' && method[i] != '&' && method[i] != '\''
		   && method[i] != '*' && method[i] != '+' && method[i] != '.' && method[i] != '^'
		   && method[i] != '_' && method[i] != '`' && method[i] != '|' && method[i] != '~')
		{
			return false;
		}
	}
	_method = method;
	return true;
}

bool HttpParser::checkTargetAndVersion(const std::string& request)
{
	size_t firstSpace = request.find(' ');

	if(firstSpace == std::string::npos)
		return false;
	if(request[firstSpace + 1] != '/')
		return false;

	size_t secondSpace = request.find(' ', firstSpace + 1);
	if(secondSpace == std::string::npos)
		return false;

	std::string version = request.substr(secondSpace + 1, 10);
	if(version != "HTTP/1.1\r\n")
		return false;

	std::string target = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	std::string illegal = "<>\"{}|\\^` ";
	for(size_t i = 0; i < target.size(); i++)
	{
		if(illegal.find(target[i]) != std::string::npos)
			return false;
	}
	_target = target;

	version = request.substr(secondSpace + 1, 8);
	_version = version;
	
	return true;
}

HttpParser::HttpParser(){}

HttpParser::~HttpParser(){}