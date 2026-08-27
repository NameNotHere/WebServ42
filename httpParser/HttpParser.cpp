#include "HttpParser.hpp"

//need to fix outofbouns for GET

void HttpParser::parseHttpRequest(const std::string& request)
{
	if(!checkMethod(request) || !checkTargetAndVersion(request))
		std::cerr << "Invalid HTTP Request!\n";
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

	if(request[secondSpace + 1] != 'H' || request[secondSpace + 2] != 'T' 
		|| request[secondSpace + 3] != 'T' || request[secondSpace + 4] != 'P'
		|| request[secondSpace + 5] != '/' || request[secondSpace + 6] != '1'
		|| request[secondSpace + 7] != '.' || request[secondSpace + 8] != '1'
		|| request[secondSpace + 9] != '\r' || request[secondSpace + 10] != '\n')
		return false;

	std::string target = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	_target = target;
	// std::cout << target << "\n";
	std::string version = request.substr(secondSpace + 1, 8);
	_version = version;
	// std::cout << version << "\n";
	
	return true;
}

HttpParser::HttpParser(){}

HttpParser::~HttpParser(){}