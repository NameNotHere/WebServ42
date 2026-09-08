#include "HttpParser.hpp"

HttpParser::BodyStatus HttpParser::body(const std::string& request)
{
	size_t start = request.find("\r\n\r\n");
	if(start == std::string::npos)
   		return BODY_INVALID;

	std::string body = request.substr(start + 4);

	std::map<std::string, std::string>::const_iterator it;
	it = _headers.find("content-length");

	bool hasContentLength = _headers.find("content-length") != _headers.end();
	bool hasTransferEncoding = _headers.find("transfer-encoding") != _headers.end();

	if(!hasContentLength && !hasTransferEncoding && !body.empty())
		return BODY_INVALID;

	if(hasTransferEncoding) // Takes precendence of both Transfer encoding and content length are present
	{
		std::cout << "Handling Tranfer-Encoding\n";
		if(!transferEncoding(body))
			return BODY_INVALID;
		// Handle Transfer Encoding chunked
	}
	else if(hasContentLength)
	{
		// Checked if this fails already in the program
		unsigned long contentLength = stoul(it->second);
		std::cout << "ContentLength:" << contentLength << "\n";

		if(contentLength == body.size())
		{
			// Body complete
			std::cout << "Body just right\n";
			return BODY_VALID;
		}
		if(contentLength < body.size())
		{
			// Send error 400 or sumshit
			std::cout << "Body too Large\n";
			return BODY_INVALID;
		}
		if(contentLength > body.size())
		{
			// Have to wait and see if more data will come 
			// Will have to implement recieve() to keep
			// checking if there is more data
			// If body.size() is finished and != to content length
			// Return error
			return BODY_INCOMPLETE;
		}
	}
	return BODY_VALID;
}

bool HttpParser::transferEncoding(const std::string& bodyValue)
{
	std::string body = bodyValue;
	size_t digit = 0;

	for(size_t i = 0; i < body.size(); i++)
	{
		if(body[i] == '\r' && body[i + 1] == '\n' 
		  && body[i + 2] == '\r' && body[i + 3] == '\n')
			break;

		size_t bytesPos = body.find("\r\n");
		if(bytesPos == std::string::npos)
			return false;

		std::string bytes = body.substr(0, bytesPos);
		std::cout << "Bytes:" << bytes << "\n";

		body = body.substr(bytesPos + 2);

		size_t valuePos = body.find("\r\n");
		if(valuePos == std::string::npos)
			return false;

		std::string value = body.substr(0, valuePos);
		std::cout << "Value:" << value << "\n";
		if(!convertBytes(bytes, digit, value))
			return false;

		body = body.substr(valuePos + 2);
	}
	return true;
}

bool HttpParser::convertBytes(std::string& bytes, size_t& digit, const std::string& value)
{
	try
	{
		digit = stoi(bytes);
		std::cout << "Digit:" << digit << "\n";
	}
	catch(const std::exception& e)
	{
		return false;
	}
	std::cout << "ValueSize:" << value.size() << "\n";
	if(digit != value.size())
		return false;
	return true;
}