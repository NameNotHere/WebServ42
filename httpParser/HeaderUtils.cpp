#include "HttpParser.hpp"

void HttpParser::ftTrim(std::string& value)
{
	size_t start = value.find_first_not_of(" \t");
	if(start != std::string::npos)
		value = value.substr(start);

	size_t end = value.find_last_not_of(" \t");
	if(end != std::string::npos)
		value = value.substr(0, end + 1);
}

bool HttpParser::checkWhiteSpace(const std::string& value)
{
	size_t spaces = value.find(" ");
	if(spaces != std::string::npos)
		return false;

	spaces = value.find("\t");
	if(spaces != std::string::npos)
		return false;

	return true;
}

void HttpParser::lowerCase(std::string& str)
{
	for(size_t i = 0; i < str.size(); i++)
	{
		str[i] = tolower(str[i]);
	}
}