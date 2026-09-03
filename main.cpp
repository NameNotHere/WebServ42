#include "ConfigParser/Configuration.hpp"
#include <iostream>

//void Init(const ServerConfig& serv)
//{
//
//}

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
    std::vector<ServerConfig> servers;
    parse(servers, lex(config));

    //for (size_t i = 0; i <servers.size(); i++)
        //Init(servers[i]);
    return 0;
}
