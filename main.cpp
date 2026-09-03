#include "ConfigParser/Configuration.hpp"
#include <iostream>

//void Init(const ServerConfig& serv)
//{
//
//}

void print_config(const std::vector<ServerConfig>& servers)
{
    for (const ServerConfig& server : servers)
    {
        std::cout << "Server\n";
        std::cout << "  name: " << server.name << '\n';
        std::cout << "  listen: " << server.listen << '\n';
        std::cout << "  root: " << server.root << '\n';

        std::cout << "  allowed methods:";
        for (const std::string& method : server.allowed_methods)
            std::cout << ' ' << method;
        std::cout << '\n';

        for (const LocationConfig& location : server.locations)
        {
            std::cout << "  Location: " << location.path << '\n';
            std::cout << "    root: " << location.root << '\n';

            std::cout << "    allowed methods:";
            for (const std::string& method : location.allowed_methods)
                std::cout << ' ' << method;
            std::cout << '\n';
        }
    }
}

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

    print_config(servers);
    return 0;
}
