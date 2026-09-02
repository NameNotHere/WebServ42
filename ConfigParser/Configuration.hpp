#ifndef             CONFIG_HPP
#define             CONFIG_HPP

#include <fstream>
#include <sstream>
#include <vector>
#include <string>

struct  server_details
{
    int listen;
    std::string server_name;
    int root;
    int index;
    std::string allow_methods;
    size_t client_body_limit;
    std::vector<std::string> error_page;
};

enum    CONF_TOKEN
{
    LEFTBRACE,
    RIGHTBRACE,
    SEMICOLON,
    WORD
};

enum    ALLOWED_METHODS
{
    GET,
    STOP,
    PLOP
};

struct Token
{
    CONF_TOKEN  type;
    std::string word;
    size_t      line;
};

struct LocationConfig
{
    std::string path;
    std::vector<std::string> allowed_methods;
    std::string root;
    std::string index;
    std::string uploadDir;
};

struct ServerConfig
{
    std::string name;
    unsigned int listen;
    std::string root;
    std::vector<LocationConfig> locations;
    std::vector<std::string> allowed_methods; //one day itll be an enum vector but not today
};

std::vector<Token>	lex(const std::string& text);

void				expect_and_increase(const std::vector<Token>& tokens, size_t& pos, CONF_TOKEN type);

#endif