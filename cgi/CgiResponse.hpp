#ifndef CGI_RESPONSE_HPP
#define CGI_RESPONSE_HPP

#include <string>
#include <map>

struct CgiResponse
{
    int          status_code;
    std::string  status_message;
    std::map<std::string, std::string> headers;
    std::string  body;

    CgiResponse();
    bool parse(const std::string& raw);
};

#endif