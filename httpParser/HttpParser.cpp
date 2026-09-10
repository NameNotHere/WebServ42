#include "HttpParser.hpp"

// Change return type to Request enum, then if body incomplete return request incomplete to caller

HttpParser::RequestStatus HttpParser::parseHttpRequest(const std::string& request)
{
    if (!requestLine(request))
    {
        std::cout << "Invalid HTTP Request!\n";
        return REQUEST_INVALID;
    }

    std::cout << "Valid HTTP Request!\n";

    if (!headers(request))
    {
        std::cout << "Invalid Header\n";
        return REQUEST_INVALID;
    }

    std::cout << "Valid Header\n";

    BodyStatus status = body(request);

    if (status == BODY_VALID)
    {
        std::cout << "Valid Body\n";
        return REQUEST_VALID;
    }
    else if (status == BODY_INVALID)
    {
        std::cout << "Invalid Body\n";
        return REQUEST_INVALID;
    }
    else if (status == BODY_INCOMPLETE)
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

const std::map<std::string, std::string>& HttpParser::getHeaders() const
{
    return _headers;
}

HttpParser::HttpParser() : _requestLength(0){}

HttpParser::HttpParser(const HttpParser& other)
	: _method(other._method), _target(other._target), _version(other._version), _requestLength(other._requestLength){}

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