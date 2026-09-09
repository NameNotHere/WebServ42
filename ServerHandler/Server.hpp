#ifndef SERVER_HPP
#define SERVER_HPP

#include "../httpParser/HttpParser.hpp"
#include "../ConfigParser/Configuration.hpp"
#include <unistd.h>
#include <iostream>
#include <poll.h>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct Server
{
    int serverFD;
    ServerConfig conf;
};

void print_config(const std::vector<ServerConfig>& servers);
void Init(Server& server);
void runEventLoop(std::vector<Server>& hosting, std::vector<pollfd>& fds, std::map<int, std::string> &reqs);
void handleNewConnection (int serverFD, const ServerConfig& config, std::vector<pollfd>& fds);
std::vector<pollfd> createPollFds(const std::vector<Server>& hosting);

#endif
