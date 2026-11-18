#include "HttpParser.hpp"
#include <utility>

HttpParser::RequestStatus HttpParser::parseHttpRequest(std::string& request, size_t maxBody)
{
    _headers.clear();
    _requestBufferConsumed = false;

    RequestStatus requestStatus = requestLine(request);

    if (requestStatus == REQUEST_BAD)
    {
        return REQUEST_BAD;
    }
    else if (requestStatus == REQUEST_METHOD_NOT_ALLOWED)
    {
        return REQUEST_METHOD_NOT_ALLOWED;
    }
    else if (requestStatus == REQUEST_METHOD_NOT_IMPLEMENTED)
    {
        return REQUEST_METHOD_NOT_IMPLEMENTED;
    }
    else if (requestStatus == REQUEST_TARGET_NOT_FOUND)
    {
        return REQUEST_TARGET_NOT_FOUND;
    }
    else if (requestStatus == REQUEST_VERSION_NOT_SUPPORTED)
    {
        return REQUEST_VERSION_NOT_SUPPORTED;
    }

    if (!headers(request))
    {
        return REQUEST_HEADER_INVALID;
    }

    BodyStatus bodyStatus = body(request, maxBody);

    if (bodyStatus == BODY_VALID)
    {
        return REQUEST_VALID;
    }
    else if (bodyStatus == BODY_INVALID)
    {
        return REQUEST_BODY_INVALID;
    }
    else if (bodyStatus == BODY_INCOMPLETE)
    {
        return REQUEST_INCOMPLETE;
    }
    else if (bodyStatus == BODY_TOO_LARGE)
        return REQUEST_CONTENT_TOO_LARGE;
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

bool HttpParser::requestBufferConsumed() const
{
    return _requestBufferConsumed;
}

const std::string& HttpParser::getBody() const
{
	return _body;
}

std::string HttpParser::takeBody()
{
	return std::move(_body);
}

const std::string& HttpParser::getPortNum() const
{
	return _portNum;
}

const std::map<std::string, std::string>& HttpParser::getHeaders() const
{
    return _headers;
}

HttpParser::HttpParser() : _requestLength(0), _requestBufferConsumed(false){}

HttpParser::HttpParser(const HttpParser& other)
    : _requestLength(other._requestLength), _requestBufferConsumed(other._requestBufferConsumed),
      _method(other._method), _target(other._target), _version(other._version), _body(other._body),
      _portNum(other._portNum), _headers(other._headers){}

HttpParser& HttpParser::operator=(const HttpParser& other)
{
	if (this != &other)
	{
		_method = other._method;
		_target = other._target;
		_version = other._version;
		_requestLength = other._requestLength;
		_requestBufferConsumed = other._requestBufferConsumed;
		_body = other._body;
		_portNum = other._portNum;
		_headers = other._headers;
	}
	return *this;
}

HttpParser::~HttpParser(){}
