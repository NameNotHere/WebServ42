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
		size_t start = value.find_first_not_of(" \t");
		if(start != std::string::npos)
			value = value.substr(start);
		std::cout << "Value after start trim = " << value << "\n";

		size_t end = value.find_last_not_of(" \t");
		if(end != std::string::npos)
			value = value.substr(0, end + 1);
		std::cout << "Value after end trim = " << value << "\n";

		// Check for white space within value
		// size_t spaces = value.find(" ");
		// if(spaces != std::string::npos)
		// 	return false;
		// spaces = value.find("\t");
		// if(spaces != std::string::npos)
		// 	return false;
		
		if(value.find("\n") != std::string::npos || value.find("\r") != std::string::npos)
			return false;

		lowerCase(name);
		_headers[name] = value;

		std::cout << "Name = " << name << "\n";
		std::cout << "Value = " << value << "\n\n";

		posStart = posFin;
		if(request[posStart + 2] == '\r' && request[posStart + 3] == '\n')
			break;
	}

	return true;
}

void HttpParser::lowerCase(std::string& str)
{
	for(size_t i = 0; i < str.size(); i++)
	{
		str[i] = tolower(str[i]);
	}
}
