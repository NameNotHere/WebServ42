#ifndef POST_HPP
#define POST_HPP

#include "../httpParser/HttpParser.hpp"

class Post {
private:

public:
	Post();
	Post(const Post& other);
	Post& operator=(const Post& other);
	~Post();
	
	std::string handleRequest(const HttpParser& request);
};

#endif