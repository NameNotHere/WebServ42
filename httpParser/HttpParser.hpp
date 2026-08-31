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

	void parseHttpRequest(const std::string& request);
	bool requestLine(const std::string& request);
	bool checkMethod(const std::string& request);
	bool checkTargetAndVersion(const std::string& request);
	bool validChar(const std::string& method);

	bool headers(const std::string& request);
	void lowerCase(std::string& str);
	void ftTrim(std::string& value);

};

#endif