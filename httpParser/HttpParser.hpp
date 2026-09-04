#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <map>
#include <string>
#include <iostream>

class HttpParser {
private:
	std::string _method;
	std::string _target;
	std::string _version;
	std::map<std::string, std::string> _headers;

public:
	HttpParser();
	HttpParser(const HttpParser& other);
	HttpParser& operator=(const HttpParser& other);
	~HttpParser();
	
	// Request Line
	void parseHttpRequest(const std::string& request);
	bool requestLine(const std::string& request);
	bool checkMethod(const std::string& request);
	bool checkTargetAndVersion(const std::string& request);
	bool validChar(const std::string& method);
	const std::string& getMethod() const;
	const std::string& getTarget() const;
	const std::string& getVersion() const;
	const std::map<std::string, std::string>& getHeaders() const;
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
	// Body
	bool body(const std::string& request);
};

#endif