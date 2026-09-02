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
		
		size_t colon = header.find(":");
		if(colon == std::string::npos)
			return false;
		
		std::string name = header.substr(0, colon);
		std::string value = header.substr(colon + 1);
		std::cout << "Header:" << header << "\n";

		if(!validChar(name))
			return false;

		// Trim white space from both ends
		ftTrim(value);

		if (!checkFieldValue(value))
    		return false;

		lowerCase(name);
	
		if (_headers.find(name) != _headers.end())
   			return false;
	
		_headers[name] = value;

		if(!checkValue(name, value))
			return false;

		std::cout << "Name:" << name << "\n";
		std::cout << "Value:" << value << "\n\n";

		posStart = posFin;
		if(request[posStart + 2] == '\r' && request[posStart + 3] == '\n')
			break;
	}
	return true;
}

bool HttpParser::checkValue(const std::string& name, const std::string& value)
{
    if (name == "host")
        return checkHost(value);
    if (name == "content-length")
        return checkContentLength(value);
    if (name == "transfer-encoding")
        return checkTransferEncoding(value);
    if (name == "content-type")
        return checkContentType(value);
    if (name == "connection")
        return checkConnection(value);
    if (name == "expect")
        return checkExpect(value);
    if (name == "user-agent")
        return checkUserAgent(value);

    return true;
}
