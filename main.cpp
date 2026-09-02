#include "ConfigParser/Configuration.hpp"
#include <iostream>

int main(int argc, char** argv)
{
	ServerConfig con;

    if (argc != 2)
    {
        std::cerr << "Usage: ./parser config.conf\n";
        return 1;
    }
    std::string conf = argv[1];
    std::ifstream file(conf);
    std::stringstream buffer;
    std::string config;

    if (!file || conf.size() < 5 || 
        conf.compare(conf.size() - 5, 5, ".conf") != 0)
    {
        std::cout << "Needs to be .conf\n";
        return 1;
    }

    buffer << file.rdbuf();
    config = buffer.str();
    
    std::cout << "-------------------LEX AND TOK-----------\n";
    std::vector<Token> lexxer = lex(config);
    for (long unsigned int i = 0; i < lexxer.size(); i++)
        std::cout << lexxer[i].type  << "  " << lexxer[i].word << "\n";
    //parse
    std::vector<ServerConfig> servers;
    parse(servers, lexxer);

    return 0;
}
