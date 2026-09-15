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

	if(!hasContentLength && !hasTransferEncoding)
	{
    	_requestLength = start + 4;
    	return BODY_VALID;
	}

	// if(!hasContentLength && !hasTransferEncoding && !body.empty())
	// 	return BODY_INVALID;

	if(hasTransferEncoding) // Takes precendence if both Transfer encoding and content length are present
	{
		size_t bodyLength = 0;
		BodyStatus status = transferEncoding(body, bodyLength);

		if(status == BODY_INVALID)
			return BODY_INVALID;
		if(status == BODY_INCOMPLETE)
			return BODY_INCOMPLETE;
		std::cout << "Body length: " << bodyLength << "\n";
		_requestLength = start + 4 + bodyLength;
		return BODY_VALID;
	}
	else if(hasContentLength)
	{
		std::map<std::string, std::string>::const_iterator it;
		it = _headers.find("content-length");
		// Checked if this fails already in the program
		unsigned long contentLength = stoul(it->second);

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

HttpParser::BodyStatus HttpParser::transferEncoding(const std::string& bodyValue, size_t& bodyLength)
{
    size_t pos = 0;

    while (true)
    {
        size_t bytesPos = bodyValue.find("\r\n", pos);

        if (bytesPos == std::string::npos)
            return BODY_INCOMPLETE;

        std::string bytes = bodyValue.substr(pos, bytesPos - pos);

        if (bytes.empty())
            return BODY_INVALID;

        size_t digit = 0;

		if(!convertBytes(bytes, digit))
			return BODY_INVALID;

        pos = bytesPos + 2;

        if (digit == 0)
        {
            if (bodyValue.size() < pos + 2)
                return BODY_INCOMPLETE;

            if (bodyValue.substr(pos, 2) != "\r\n")
                return BODY_INVALID;

            pos += 2;
            bodyLength = pos;
            return BODY_VALID;
        }

        if (bodyValue.size() < pos + digit + 2)
            return BODY_INCOMPLETE;

        pos += digit;

        if (bodyValue.substr(pos, 2) != "\r\n")
            return BODY_INVALID;

        pos += 2;
    }
}

bool HttpParser::convertBytes(std::string& bytes, size_t& digit)
{
	try
	{
		digit = stoul(bytes, 0, 16);
	}
	catch(const std::exception& e)
	{
		return false;
	}
	return true;
}

// HttpParser::BodyStatus HttpParser::transferEncoding(const std::string& bodyValue, size_t& bodyLength)
// {
// 	std::string body = bodyValue;
// 	size_t digit = 0;
// 	size_t i = 0;

// 	for(i = 0; i < body.size(); i++)
// 	{
// 		if(body[i] == '\r' && body[i + 1] == '\n' 
// 		  && body[i + 2] == '\r' && body[i + 3] == '\n')
// 			break;

// 		size_t bytesPos = body.find("\r\n");
// 		if(bytesPos == std::string::npos)
// 			return BODY_INCOMPLETE;

// 		std::string bytes = body.substr(0, bytesPos);
// 		std::cout << "Bytes:" << bytes << "\n";

// 		body = body.substr(bytesPos + 2);

// 		size_t valuePos = body.find("\r\n");
// 		if(valuePos == std::string::npos)
// 			return BODY_INCOMPLETE;

// 		std::string value = body.substr(0, valuePos);
// 		std::cout << "Value:" << value << "\n";
	
// 		if(!convertBytes(bytes, digit, value))
// 			return BODY_INVALID;
// 		if(digit == 0)
// 		{
// 			bodyLength = bodyValue.size() - body.size();
//     		bodyLength += 2;
// 			break;
// 		}
// 		body = body.substr(valuePos + 2);
// 	}
// 	if(body[i] != '\r' && body[i - 1] != '\n' 
// 	  && body[i - 2] != '\r' && body[i - 3] != '\n')
// 		return BODY_INVALID;
// 	return BODY_VALID;
// }