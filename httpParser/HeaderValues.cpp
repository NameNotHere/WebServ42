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
	std::cout << "Hostname:" << hostname << "\n";

	for(size_t i = 0; i < hostname.size(); i++)
	{
		if(!isalnum(hostname[i]) && hostname[i] != '.' && hostname[i] != '-')
			return false;
	}

	std::string port = value.substr(colon + 1);
	if(port.empty())
		return false;

	std::cout << "Port:" << port << "\n";
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

bool HttpParser::checkContentLength(const std::string& value)
{
	if(value.empty())
		return false;
	for(size_t i = 0; i < value.size(); i++)
	{
		if(!isdigit(value[i]))
			return false;
	}
	return true;
}

bool HttpParser::checkContentType(const std::string& value)
{
	size_t semi = value.find(";");

	if(semi != std::string::npos)
	{
		std::string mediaType = value.substr(0, semi);

		if(!checkMediaType(mediaType))
			return false;
		std::cout << "Media type:" << mediaType << "\n";
		
		std::string paramList = value.substr(semi + 1);
		ftTrim(paramList);
		std::cout << "ParamList:" << paramList << "\n";
		while(!paramList.empty())
		{
			size_t nextSemi = paramList.find(";");
			std::string param;
			if(nextSemi == std::string::npos)
			{
				param = paramList;
				paramList.clear();
			}
			else
			{
				param = paramList.substr(0, nextSemi);
				paramList = paramList.substr(nextSemi + 1);
				ftTrim(paramList);
			}
			if(!checkMediaParam(param))
				return false;
			std::cout << "Param:" << param << "\n";
		}
	}
	else
	{
		std::string mediaType = value.substr(0, semi);
		std::cout << "Media type:" << mediaType << "\n";

		if(!checkMediaType(mediaType))
			return false;
	}
	return true;
}

bool HttpParser::checkMediaType(const std::string& mediaType)
{
	size_t slash = mediaType.find("/");

	if(slash == std::string::npos)
		return false;
	
	if(slash == 0 || slash == mediaType.size() - 1)
		return false;

	if(mediaType.find("/", slash + 1) != std::string::npos)
		return false;
	
	std::string rawType = mediaType.substr(0, slash);
	std::string subType = mediaType.substr(slash + 1);

	if(!validChar(rawType) || !validChar(subType))
		return false;

	return true;
}

bool HttpParser::checkMediaParam(const std::string& param)
{
	size_t equals = param.find("=");

	if(equals == std::string::npos)
		return false;

	std::string name = param.substr(0, equals);
	std::cout << "ParamName:" << name << "\n";
	std::string value = param.substr(equals + 1);
	std::cout << "ParamValue:" << value << "\n";

	if(!validChar(name) || !validChar(value))
		return false;
	return true;
}

bool HttpParser::checkTransferEncoding(const std::string& value)
{
	std::string chunked = value;

	lowerCase(chunked);
	if(chunked != "chunked")
		return false;
	return true;
}

bool HttpParser::checkUserAgent(const std::string& value)
{
	(void) value;
	return true;
}

bool HttpParser::checkExpect(const std::string& value)
{
	(void) value;
	return true;
}