#include "ServerHandler/Server.hpp"
#include "ConfigParser/Configuration.hpp"

#include <fstream>
#include <sstream>
#include <vector>
#include <poll.h>
#include <arpa/inet.h>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2)
        return std::cerr << "Usage: ./webserv config.conf\n", 1;
    std::string conf = argv[1];
    if (conf.size() < 5 || conf.compare(conf.size() - 5, 5, ".conf") != 0)
        return std::cerr << "Config file needs to be .conf\n", 1;
    std::ifstream file(conf);
    if (!file)
        return std::cerr << "Could not open config file\n", 1;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string config = buffer.str();
    std::vector<ServerConfig> configs;

    parse(configs, lex(config));
    print_config(configs);

    std::vector<Server> hosting;
    for (size_t i = 0; i < configs.size(); i++)
    {
        Server server;
        server.serverFD = -1;
        server.conf = configs[i];
        Init(server);
        hosting.push_back(server);
    }

    if (hosting.empty())
        return std::cerr << "No server configured\n" << std::endl, 1;

    std::vector<pollfd> fds;
    fds.reserve(hosting.size());
    for (size_t i = 0; i < hosting.size(); ++i)
    {
        pollfd p;
        p.fd = hosting[i].serverFD;
        p.events = POLLIN;
        p.revents = 0;
        fds.push_back(p);
    }

    // Accept clients on any listening socket
    while (true)
    {
        int ready = poll(fds.data(), fds.size(), -1);
        if (ready <= 0)
            continue;

        for (size_t i = 0; i < fds.size(); ++i)
        {
            if (fds[i].revents & POLLIN)
            {
                sockaddr_in clientAddr;
                socklen_t addrlen = sizeof(clientAddr);
                int clientFD = accept(fds[i].fd, reinterpret_cast<sockaddr*>(&clientAddr), &addrlen);
                if (clientFD == -1)
                    continue;

                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));
                std::cout << "Client connected from " << ip << ":" << ntohs(clientAddr.sin_port)<< " to port " << hosting[i].conf.listen << std::endl;
            }
        }
    }
    return 0;
}
