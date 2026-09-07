#include "HttpParser.hpp"

bool HttpParser::body(const std::string& request)
{
	size_t start = request.find("\r\n\r\n");
	if(start == std::string::npos)
   		return false;

	std::string body = request.substr(start + 4);
	std::cout << "Body:" << body << "\n";

	std::map<std::string, std::string>::const_iterator it;
	it = _headers.find("content-length");

	bool hasContentLength = _headers.find("content-length") != _headers.end();
	bool hasTransferEncoding = _headers.find("transfer-encoding") != _headers.end();

	if(!hasContentLength && !hasTransferEncoding && !body.empty())
		return false;

	if(hasTransferEncoding) // Takes precendence of both Transfer encoding and content length are present
	{
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
			return true;
		}
		if(contentLength < body.size())
		{
			// Send error 400 or sumshit
			std::cout << "Body too Large\n";
			return false;
		}
		if(contentLength > body.size())
		{
			// Have to wait and see if more data will come 
			// Will have to implement recieve() to keep
			// checking if there is more data
			// If body.size() is finished and != to content length
			// Return error
			std::cout << "Body incomplete\n";
			return true; // Temporarily
		}
	}
	return true;
}