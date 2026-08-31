#include "HttpParser.hpp"

// Need to hande IPV6

bool HttpParser::checkHost(const std::string& value)
{
	if(value.empty())
		return false;
	if(!checkWhiteSpace(value))
		return false;

	size_t colon = value.find(":");
	if(colon == std::string::npos)
		return false;

	std::string hostname = value.substr(0, colon);
	std::cout << "Hostname = " << hostname << "\n";

	for(size_t i = 0; i < hostname.size(); i++)
	{
		if(!isalnum(hostname[i]) && hostname[i] != '.' && hostname[i] != '-')
			return false;
	}

	std::string port = value.substr(colon + 1);
	if(port.empty())
		return false;

	std::cout << "Port = " << port << "\n";
	for(size_t i = 0; i < port.size(); i++)
	{
		if(!isdigit(port[i]))
			return false;
	}

	try
	{
		int portnum = stoi(port);
		if(portnum > 65535) // Max TCP range
			return false;
	}
	catch(const std::exception& e)
	{
		return false;
	}
	return true;
}

bool HttpParser::checkConnection(const std::string& value)
{
	std::string connection = value;

	lowerCase(connection);
	if(connection != "keep-alive" && connection != "close")
		return false;

	return true;
}

bool HttpParser::checkUserAgent(const std::string& value)
{
	(void) value;
	return true;
}

bool HttpParser::checkContentLength(const std::string& value)
{
	(void) value;
	return true;
}

bool HttpParser::checkTransferEncoding(const std::string& value)
{
	(void) value;
	return true;
}

bool HttpParser::checkContentType(const std::string& value)
{
	(void) value;
	return true;
}

bool HttpParser::checkExpect(const std::string& value)
{
	(void) value;
	return true;
}