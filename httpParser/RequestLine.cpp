#include "HttpParser.hpp"

HttpParser::RequestStatus HttpParser::requestLine(const std::string& request)
{
	if(checkMethod(request) == REQUEST_BAD)
		return REQUEST_BAD;

	else if(checkMethod(request) == REQUEST_METHOD_NOT_ALLOWED)
		return REQUEST_METHOD_NOT_ALLOWED;

	if(!checkTarget(request))
		return REQUEST_TARGET_NOT_FOUND;

	if(!checkVersion(request))
		return REQUEST_VERSION_NOT_SUPPORTED;

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

	lowerCase(method);
	if (method != "get"    &&
		method != "head"   &&
		method != "post"   &&
		method != "put"    &&
		method != "delete" &&
		method != "options"&&
		method != "trace"  &&
		method != "connect"&&
		method != "patch"   )
		return REQUEST_BAD;
	if (method != "get"    &&
		method != "post"   &&
		method != "delete"  )
		return  REQUEST_METHOD_NOT_ALLOWED;
	
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

bool HttpParser::checkVersion(const std::string& request)
{
	size_t firstSpace = request.find(' ');

	if(firstSpace == std::string::npos)
		return false;

	size_t secondSpace = request.find(' ', firstSpace + 1);
	if(secondSpace == std::string::npos)
		return false;

	std::string version = request.substr(secondSpace + 1, 10);
	if(version != "HTTP/1.1\r\n")
		return false;

	version = request.substr(secondSpace + 1, 8);
	_version = version;

	return true;
}