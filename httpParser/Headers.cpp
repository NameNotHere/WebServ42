#include "HttpParser.hpp"

bool HttpParser::headers(const std::string& request)
{
	size_t posStart = request.find("\r");
	size_t posFin = request.find("\r", posStart + 2);
	if(posFin == std::string::npos)
		return false;

	std::string firstH = request.substr(posStart + 2, posFin - posStart - 2);
	std::cout << firstH << "\n";
	
	size_t colon = firstH.find(":");
	if(colon == std::string::npos)
		return false;
	
	std::string name = firstH.substr(0, colon);
	std::string value = firstH.substr(colon + 2, posFin - posStart - 2);
	std::cout << "Name = " << name << "\n";
	std::cout << "Value = " << value << "\n";
	if(!validChar(name))
		return false;
	//Next check Value
	return true;
}
