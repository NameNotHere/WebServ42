#ifndef SERVER_HPP
#define SERVER_HPP

#include "ConfigParser/Configuration.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>

struct Server
{
    int serverFD;
    ServerConfig conf;
};

void print_config(const std::vector<ServerConfig>& servers);
void Init(Server& server);

#endif
