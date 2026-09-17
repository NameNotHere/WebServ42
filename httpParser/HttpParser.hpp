#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <map>
#include <string>
#include <unistd.h>
#include <iostream>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>

class HttpParser {
private:
	size_t      _requestLength;
	std::string _method;
	std::string _target;
	std::string _version;
	std::string _body;
	std::map<std::string, std::string> _headers;

public:
	enum RequestStatus
	{
		REQUEST_BAD,
		REQUEST_VALID,
		REQUEST_INVALID,
		REQUEST_NOT_FOUND,
		REQUEST_INCOMPLETE,
		REQUEST_BODY_INVALID,
		REQUEST_HEADER_INVALID,
		REQUEST_TARGET_NOT_FOUND,
		REQUEST_CONTENT_TOO_LARGE,
		REQUEST_METHOD_NOT_ALLOWED,
		REQUEST_VERSION_NOT_SUPPORTED
	};

	enum BodyStatus
	{
		BODY_VALID,
		BODY_INVALID,
		BODY_INCOMPLETE
	};

	HttpParser();
	HttpParser(const HttpParser& other);
	HttpParser& operator=(const HttpParser& other);
	~HttpParser();
	
	// Request Line
	RequestStatus parseHttpRequest(const std::string& request);
	RequestStatus requestLine(const std::string& request);
	bool checkMethod(const std::string& request);
	bool checkTarget(const std::string& request);
	bool checkVersion(const std::string& request);
	bool validChar(const std::string& method);
	const std::string& getMethod() const;
	const std::string& getTarget() const;
	const std::string& getVersion() const;
	const std::string& getBody() const;
	size_t getRequestLength() const;
	// Headers
	bool headers(const std::string& request);
	void ftTrim(std::string& value);
	void lowerCase(std::string& str);
	bool checkHost(const std::string& value);
	bool checkExpect(const std::string& value);
	bool checkConnection(const std::string& value);
	bool checkContentType(const std::string& value);
	bool checkContentLength(const std::string& value);
	bool checkTransferEncoding(const std::string& value);
	bool checkValue(const std::string& name, const std::string& value);
	bool checkUserAgent(const std::string& value);
	bool checkWhiteSpace(const std::string& value);
	bool checkFieldValue(const std::string& value);
	bool checkMediaType(const std::string& mediaType);
	bool checkMediaParam(const std::string& param);
	bool validValue(const std::string& value);
	const std::map<std::string, std::string>& getHeaders() const;

	// Body
	BodyStatus body(const std::string& request);
	BodyStatus transferEncoding(const std::string& bodyValue, size_t& bodyLength);
	bool convertBytes(std::string& bytes, size_t& digit);
};

#endif