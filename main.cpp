#include "ServerHandler/Server.hpp"
#include "ConfigParser/Configuration.hpp"

#include <fstream>
#include <sstream>
#include <vector>

#include <iostream>
#include <unistd.h>
#include <cstring>

int main(int argc, char** argv)
{
    if (argc != 2)
        return std::cerr << "Usage: ./webserv config.conf\n", 1;

    std::string conf = argv[1];
    if (conf.size() < 5 ||
        conf.compare(conf.size() - 5, 5, ".conf") != 0)
    {
        std::cerr << "Config file needs to be .conf\n";
        return 1;
    }
    std::ifstream file(conf);
    if (!file)
    {
        std::cerr << "Could not open config file\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string config = buffer.str();
    std::vector<ServerConfig> configs;

    parse(configs, lex(config));
    print_config(configs);

    std::map<int, std::string> reqs;
    std::vector<Server> hosting;
    for (size_t i = 0; i < configs.size(); ++i)
    {
        Server server;
        server.serverFD = -1;
        server.conf = configs[i];
        Init(server);
        hosting.push_back(server);
    }
    if (hosting.empty())
        return std::cerr << "No server configured\n", 1;
    std::vector<pollfd> fds = createPollFds(hosting);
    runEventLoop(hosting, fds, reqs);

    return 0;
}
