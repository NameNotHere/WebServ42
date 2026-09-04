#include "HttpParser.hpp"

// Need to check for extra \r\n hanging around

bool HttpParser::body(const std::string& request)
{
	size_t start = request.find("\r\n\r\n");
	std::string body = request.substr(start + 4);
	std::cout << "Body:" << body << "\n";

	std::map<std::string, std::string>::const_iterator it;
	it = _headers.find("content-length");

	bool hasContentLength = _headers.find("content-length") != _headers.end();
	bool hasTansferEncoding = _headers.find("transfer-encoding") != _headers.end();

	if(!hasContentLength && !hasTansferEncoding && !body.empty())
		return false;

	if(it != _headers.end())
	{
		// Checked if this fails already in the program
		unsigned long contentLength = stoul(it->second);
		std::cout << "ContentLength:" << contentLength << "\n";

		if(contentLength == body.size())
		{
			// Body complete
			std::cout << "Body just right\n";
			return true;
		}
		if(contentLength < body.size())
		{
			// Send error 404 or sumshit
			std::cout << "Body too Large\n";
			return false;
		}
		if(contentLength > body.size())
		{
			// Have to wait and see if more data will come 
			std::cout << "Body too small\n";
		}
	}
	return true;
}