#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstddef>
#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

enum CONF_TOKEN
{
    LEFTBRACE,
    RIGHTBRACE,
    SEMICOLON,
    WORD
};

enum ALLOWED_METHODS
{
    GET,
    POST,
    DELETE
};

struct Token
{
    CONF_TOKEN type;
    std::string word;
    size_t line;
};

struct LocationConfig
{
    std::string path;
    std::vector<std::string> allowed_methods;
    std::string root;
    std::string index;
    std::string uploadDir;
    std::string cgi_path;
    unsigned int redirectCode = 0;
    std::string redirectTarget;
    size_t maxBody =        0;
    bool hasMaxBody =       false;
    bool autoindex =        false;

    //LocationConfig() : maxBody(0), hasMaxBody(false), autoindex(false) {}
};

struct ServerConfig
{
    std::string name;
    std::map<int, std::string> errors;
    std::string listenAddress = "0.0.0.0";
    unsigned int listen = 0;
    std::string root;
    std::vector<LocationConfig> locations;
    std::vector<ALLOWED_METHODS> allowed_methods;
    int serverFD = -1;

    //ServerConfig() : listen(0), serverFD(-1) {}
};


std::vector<Token>      lex(const std::string& text);
void                    parse(std::vector<ServerConfig>& servers, const std::vector<Token>& tokens);
void                    expect_and_increase(const std::vector<Token>& tokens, size_t& pos, CONF_TOKEN expected);
void                    parse_server_directive(const std::vector<Token>& tokens, size_t& pos, ServerConfig& server);
void                    parse_location_directive(const std::vector<Token>& tokens, size_t& pos, LocationConfig& location);
LocationConfig          parse_location(const std::vector<Token>& tokens, size_t& pos);
ALLOWED_METHODS         parse_method(const std::string& method);
ServerConfig            parse_server(const std::vector<Token>& tokens, size_t& pos);

#endif
