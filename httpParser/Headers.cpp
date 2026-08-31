#include "HttpParser.hpp"

bool HttpParser::headers(const std::string& request)
{
	size_t posStart = request.find("\r\n");
	if(posStart == std::string::npos)
		return false;

	while(1)
	{
		size_t posFin = request.find("\r\n", posStart + 2);
		if(posFin == std::string::npos)
			return false;

		std::string header = request.substr(posStart + 2, posFin - posStart - 2);
		std::cout << "Header = " << header << "\n";
		
		size_t colon = header.find(":");
		if(colon == std::string::npos)
			return false;
		
		std::string name = header.substr(0, colon);
		std::string value = header.substr(colon + 1);

		if(!validChar(name))
			return false;

		// Trim white space from both ends
		ftTrim(value);
		
		if(value.find("\n") != std::string::npos || value.find("\r") != std::string::npos)
			return false;

		lowerCase(name);
		_headers[name] = value;
		if(!checkValue(name, value))
			return false;

		std::cout << "Name = " << name << "\n";
		std::cout << "Value = " << value << "\n\n";

		posStart = posFin;
		if(request[posStart + 2] == '\r' && request[posStart + 3] == '\n')
			break;
	}
	return true;
}

bool HttpParser::checkValue(const std::string& name, const std::string& value)
{
	if(name == "host")
	{
		if(!checkHost(value))
			return false;
		else
			return true;
	}
	if(name == "content-length")
	{
		if(!checkContentLength(value))
			return false;
		else
			return true;
	}
	if(name == "transfer-encoding")
	{
		if(!checkTransferEncoding(value))
			return false;
		else
			return true;
	}
	if(name == "content-type")
	{
		if(!checkContentType(value))
			return false;
		else
			return true;
	}
	if(name == "connection")
	{
		if(!checkConnection(value))
			return false;
		else
			return true;
	}
	if(name == "expect")
	{
		if(!checkExpect(value))
			return false;
		else
			return true;
	}
	if(name == "user-agent")
	{
		if(!checkUserAgent(value))
			return false;
		else
			return true;
	}
	return false;
}

bool HttpParser::checkHost(const std::string& value)
{
	if(!checkWhiteSpace(value))
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

bool HttpParser::checkConnection(const std::string& value)
{
	(void) value;
	return true;
}

bool HttpParser::checkExpect(const std::string& value)
{
	(void) value;
	return true;
}

void HttpParser::ftTrim(std::string& value)
{
	size_t start = value.find_first_not_of(" \t");
	if(start != std::string::npos)
		value = value.substr(start);
	std::cout << "Value after start trim = " << value << "\n";

	size_t end = value.find_last_not_of(" \t");
	if(end != std::string::npos)
		value = value.substr(0, end + 1);
	std::cout << "Value after end trim = " << value << "\n";
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
