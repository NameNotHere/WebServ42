#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <cstddef>
#include <fstream>
#include <vector>
#include <map>

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
    std::map<int, std::string> errors;
    unsigned int listen;
    std::string root;
    std::vector<LocationConfig> locations;
    std::vector<ALLOWED_METHODS> allowed_methods;
    int serverFD = -1;
};

std::vector<Token>      lex(const std::string& text);
void                    parse(std::vector<ServerConfig>& servers, const std::vector<Token>& tokens);
void                    expect_and_increase(const std::vector<Token>& tokens, size_t& pos, CONF_TOKEN expected);

#endif
