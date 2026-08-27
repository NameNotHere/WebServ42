#include "HttpParser.hpp"

void HttpParser::parseHttpRequest(const std::string& request)
{
	if(!checkMethod(request))
		std::cout << "Invalid HTTP (Method)\n";
	if(!checkTarget(request))
		std::cout << "Invalid HTTP (Target)\n";

}

bool HttpParser::checkTarget(const std::string& request)
{
	size_t firstSpace = request.find(' ');

	if(request[firstSpace + 1] != '/')
		return false;

	size_t secondSpace = request.find(' ', firstSpace + 1);
	std::string target = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	std::cout << target << "\n";
	return true;
}

bool HttpParser::checkMethod(const std::string& request)
{
	size_t pos = request.find(' ');

	if(pos == std::string::npos)
		return false;

	std::string method = request.substr(0, pos);
	std::cout << method << "\n";
	
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
	return true;
}

HttpParser::HttpParser(){}

HttpParser::~HttpParser(){}