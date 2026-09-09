#include "ConfigParser/Configuration.hpp"
#include "Server.hpp"
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

void Init(Server& server)
{
    server.serverFD = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; // option for setsockopt


    if (server.serverFD == -1)
        throw std::runtime_error("socket() failed");
    if (setsockopt(server.serverFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        close(server.serverFD);
        server.serverFD = -1;
        throw std::runtime_error("setsockopt() failed");
    }

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(server.conf.listen);

    if (bind(server.serverFD, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1)
    {
        close(server.serverFD);
        server.serverFD = -1;
        throw std::runtime_error("bind() failed");
    }
    if (listen(server.serverFD, SOMAXCONN) == -1)
    {
        close(server.serverFD);
        server.serverFD = -1;
        throw std::runtime_error("listen() failed");
    }

    std::cout << "Listening on port " << server.conf.listen << std::endl;
}

void print_config(const std::vector<ServerConfig>& servers)
{
    for (const ServerConfig& server : servers)
    {
        std::cout << "Server\n";
        std::cout << "  name: " << server.name << '\n';
        std::cout << "  listen: " << server.listen << '\n';
        std::cout << "  root: " << server.root << '\n';
        std::cout << "\t\tErrors: \n" ;
        for (auto &[key, value] : server.errors)
            std::cout << " \t" << key << " : " << value << '\n';
        std::cout << "\nallowed methods:";
        for (const ALLOWED_METHODS& method : server.allowed_methods)
            std::cout << ' ' << method;
        std::cout << '\n';

        for (const LocationConfig& location : server.locations)
        {
            std::cout << "\tLocation: " << location.path << '\n';
            std::cout << "\troot: " << location.root << '\n';

            std::cout << "    allowed methods:";
            for (const std::string& method : location.allowed_methods)
                std::cout << ' ' << method;
            std::cout << '\n';
        }
    }
}