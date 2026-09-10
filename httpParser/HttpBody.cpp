#include "HttpParser.hpp"

// Might have to check for empty body which is still valid
// Will there ever be content length or Transfer-encoding and no body?

HttpParser::BodyStatus HttpParser::body(const std::string& request)
{
	size_t start = request.find("\r\n\r\n");
	if(start == std::string::npos)
   		return BODY_INVALID;

	std::string body = request.substr(start + 4);

	bool hasContentLength = _headers.find("content-length") != _headers.end();
	bool hasTransferEncoding = _headers.find("transfer-encoding") != _headers.end();

	if(!hasContentLength && !hasTransferEncoding && !body.empty())
		return BODY_INVALID;

	if(hasTransferEncoding) // Takes precendence if both Transfer encoding and content length are present
	{
		if(!transferEncoding(body))
			return BODY_INVALID;
	}
	else if(hasContentLength)
	{
		std::map<std::string, std::string>::const_iterator it;
		it = _headers.find("content-length");
		// Checked if this fails already in the program
		unsigned long contentLength = stoul(it->second);

		if(contentLength < body.size())
		{
			// Send error 400 or sumshit
			return BODY_INVALID;
		}
		if(contentLength > body.size())
		{
			return BODY_INCOMPLETE;
		}
		_requestLength = start + 4 + contentLength;
		std::cout << "_requestLength = " << _requestLength << "\n";
		return BODY_VALID;
	}
	_requestLength = start + 4;
	return BODY_VALID;
}

bool HttpParser::transferEncoding(const std::string& bodyValue)
{
	std::string body = bodyValue;
	size_t digit = 0;
	size_t i = 0;

	for(i = 0; i < body.size(); i++)
	{
		if(body[i] == '\r' && body[i + 1] == '\n' 
		  && body[i + 2] == '\r' && body[i + 3] == '\n')
			break;

		size_t bytesPos = body.find("\r\n");
		if(bytesPos == std::string::npos)
			return false;

		std::string bytes = body.substr(0, bytesPos);
		// std::cout << "Bytes:" << bytes << "\n";

		body = body.substr(bytesPos + 2);

		size_t valuePos = body.find("\r\n");
		if(valuePos == std::string::npos)
			return false;

		std::string value = body.substr(0, valuePos);
		// std::cout << "Value:" << value << "\n";
		if(!convertBytes(bytes, digit, value))
			return false;
		if(digit == 0)
			break;
		body = body.substr(valuePos + 2);
	}
	if(body[i] != '\n' && body[i - 1] != '\r' 
	  && body[i - 2] != '\n' && body[i - 3] != '\r')
		return false;

	return true;
}

bool HttpParser::convertBytes(std::string& bytes, size_t& digit, const std::string& value)
{
	try
	{
		digit = stoi(bytes);
		// std::cout << "Digit:" << digit << "\n";
	}
	catch(const std::exception& e)
	{
		return false;
	}
	// std::cout << "ValueSize:" << value.size() << "\n";
	if(digit != value.size())
		return false;
	return true;
}