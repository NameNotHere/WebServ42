#include "HttpParser.hpp"

HttpParser::RequestStatus HttpParser::requestLine(const std::string& request)
{
	if (checkMethod(request) == REQUEST_BAD)
		return REQUEST_BAD;
	else if (checkMethod(request) == REQUEST_METHOD_NOT_IMPLEMENTED)
		return REQUEST_METHOD_NOT_IMPLEMENTED;

	if (!checkTarget(request))
		return REQUEST_TARGET_NOT_FOUND;

	if (checkVersion(request) == REQUEST_VERSION_NOT_SUPPORTED)
		return REQUEST_VERSION_NOT_SUPPORTED;
	else if (checkVersion(request) == REQUEST_BAD)
		return REQUEST_BAD;

	return REQUEST_VALID;
}

HttpParser::RequestStatus HttpParser::checkMethod(const std::string& request)
{
	size_t pos = request.find(' ');

	if(pos == std::string::npos || request[pos + 1] != '/')
		return REQUEST_BAD;

	std::string method = request.substr(0, pos);
	if(!validChar(method))
		return REQUEST_BAD;

	std::string lowerMethod = method;
	lowerCase(lowerMethod);

	if (lowerMethod != "get"    &&
		lowerMethod != "post"   &&
		lowerMethod != "delete"  )
		return  REQUEST_METHOD_NOT_IMPLEMENTED;
	
	_method = method;
	return REQUEST_VALID;
}

bool HttpParser::checkTarget(const std::string& request)
{
	size_t firstSpace = request.find(' ');

	if(firstSpace == std::string::npos)
		return false;

	size_t secondSpace = request.find(' ', firstSpace + 1);
	if(secondSpace == std::string::npos)
		return false;

	std::string target = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	std::string illegal = "<>\"{}|\\^` ";
	for(size_t i = 0; i < target.size(); i++)
	{
		if(illegal.find(target[i]) != std::string::npos)
			return false;
	}
	_target = target;
	
	return true;
}

HttpParser::RequestStatus HttpParser::checkVersion(const std::string& request)
{
	size_t firstSpace = request.find(' ');

	if(firstSpace == std::string::npos)
		return REQUEST_BAD;

	size_t secondSpace = request.find(' ', firstSpace + 1);
	if(secondSpace == std::string::npos)
		return REQUEST_BAD;

	std::string version = request.substr(secondSpace + 1, 10);

	if (version != "HTTP/1.1\r\n")
	{
		if (version.find("HTTP/") == 0 &&
			version.size() >= 8 &&
			version.compare(version.size() - 2, 2, "\r\n") == 0)
		{
			return REQUEST_VERSION_NOT_SUPPORTED;
		}
		return REQUEST_BAD;
	}
	_version = "HTTP/1.1";
	return REQUEST_VALID;
}