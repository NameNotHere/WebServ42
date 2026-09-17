#include "HttpParser.hpp"

HttpParser::RequestStatus HttpParser::parseHttpRequest(const std::string& request)
{
    RequestStatus requestStatus = requestLine(request);

    if (requestStatus == REQUEST_METHOD_NOT_ALLOWED)
    {
        std::cout << "Request Method Not allowed\n";
        return REQUEST_METHOD_NOT_ALLOWED;
    }
    else if (requestStatus == REQUEST_TARGET_NOT_FOUND)
    {
        std::cout << "Request Target Not found\n";
        return REQUEST_TARGET_NOT_FOUND;
    }
    else if (requestStatus == REQUEST_VERSION_NOT_SUPPORTED)
    {
        std::cout << "Http Version not supported\n";
        return REQUEST_VERSION_NOT_SUPPORTED;
    }

    std::cout << "Valid HTTP Request!\n";

    if (!headers(request))
    {
        std::cout << "Invalid Header\n";
        return REQUEST_HEADER_INVALID;
    }

    std::cout << "Valid Header\n";

    BodyStatus bodyStatus = body(request);

    if (bodyStatus == BODY_VALID)
    {
        std::cout << "Valid Body\n";
        return REQUEST_VALID;
    }
    else if (bodyStatus == BODY_INVALID)
    {
        std::cout << "Invalid Body\n";
        return REQUEST_BODY_INVALID;
    }
    else if (bodyStatus == BODY_INCOMPLETE)
    {
        std::cout << "Incomplete Body\n";
        return REQUEST_INCOMPLETE;
    }
    return REQUEST_INVALID;
}

const std::string& HttpParser::getMethod() const
{
	return _method;
}

const std::string& HttpParser::getTarget() const
{
	return _target;
}

const std::string& HttpParser::getVersion() const
{
	return _version;
}

size_t HttpParser::getRequestLength() const
{
    return _requestLength;
}

const std::string& HttpParser::getBody() const
{
	return _body;
}

const std::map<std::string, std::string>& HttpParser::getHeaders() const
{
    return _headers;
}

HttpParser::HttpParser() : _requestLength(0){}

HttpParser::HttpParser(const HttpParser& other)
    : _requestLength(other._requestLength), _method(other._method), _target(other._target), _version(other._version){}

HttpParser& HttpParser::operator=(const HttpParser& other)
{
	if (this != &other)
	{
		_method = other._method;
		_target = other._target;
		_version = other._version;
		_requestLength = other._requestLength;
	}
	return *this;
}

HttpParser::~HttpParser(){}