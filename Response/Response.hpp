#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>

class Response {
private:

public:
	Response();
	Response(int statusCode, const std::string& body);
	Response(const Response& other);
	Response& operator=(const Response& other);
	~Response();

	static std::string create(int statusCode, const std::string& body);
	static std::string statusMsg(int statusCode);
};

#endif