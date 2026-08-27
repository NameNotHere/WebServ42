#include "HttpParser.hpp"

void HttpParser::parseHttpRequest(const std::string& request)
{
	size_t pos = request.find(' ');

	if(pos == std::string::npos)
		std::cout << "Invalid HTTP (Method)\n";

	std::string method = request.substr(0, pos);
	std::cout << method << "\n";

	if(!checkMethod(method))
		std::cout << "Invalid HTTP (Method)\n";
}

bool HttpParser::checkMethod(const std::string& method)
{
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