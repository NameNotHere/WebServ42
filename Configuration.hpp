#ifndef             CONFIG_HPP
#define             CONFIG_HPP

#include <fstream>
#include <sstream>
#include <vector>
#include <string>


/*BLOCKS:
    server
    location

SERVER DIRECTIVES:
    listen
    server_name
    root
    index
    allow_methods
    client_body_limit
    error_page

LOCATION DIRECTIVES:
    root
    index
    allow_methods
    autoindex
    return
    upload_dir
    cgi
*/

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

void expect(
    const std::vector<Token>& tokens,
    size_t& pos,
    CONF_TOKEN type)
{
    if (pos >= tokens.size() || tokens[pos].type != type)
        throw std::runtime_error("unexpected token");

    pos++;
}

#endif