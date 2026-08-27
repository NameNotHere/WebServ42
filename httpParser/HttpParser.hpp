#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <string>
#include <iostream>

class HttpParser {
private:
	std::string _method;
	std::string _target;
	std::string _version;

public:
	HttpParser();
	~HttpParser();

	void parseHttpRequest(const std::string& request);
	bool checkMethod(const std::string& request);
	bool checkTarget(const std::string& request);

};

#endif