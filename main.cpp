#include "ServerHandler/Server.hpp"
#include "ConfigParser/Configuration.hpp"

#include <fstream>
#include <sstream>
#include <vector>
#include <poll.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <cstring>

// Create pollfd entries for all listening sockets
std::vector<pollfd> createPollFds(const std::vector<Server>& hosting)
{
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

    return fds;
}

// Accept a new client from a listening socket
void handleNewConnection (int serverFD, const ServerConfig& config, std::vector<pollfd>& fds)
{
    sockaddr_in clientAddr;
    pollfd client;
    char ip[INET_ADDRSTRLEN];

    socklen_t addrlen = sizeof(clientAddr);
    int clientFD = accept(serverFD, reinterpret_cast<sockaddr*>(&clientAddr), &addrlen);
    if (clientFD == -1)
    {
        std::cerr << "accept() failed\n";
        return;
    }

    if (inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip)) == NULL)
    {
        std::cerr << "inet_ntop() failed\n";
        close(clientFD);
        return;
    }
    client.fd = clientFD;
    client.events = POLLIN;
    client.revents = 0;
    fds.push_back(client);
    std::cout << "Client connected from " << ip << ":" << ntohs(clientAddr.sin_port) << " to port " << config.listen << std::endl;
}


// Main event loop
void runEventLoop(std::vector<Server>& hosting, std::vector<pollfd>& fds)
{
    while (true)
    {
        int ready = poll(fds.data(), fds.size(), -1);
        if (ready == -1)
        {
            std::cerr << "poll() failed\n";
            break;
        }

        for (size_t i = 0; i < fds.size(); ++i)
        {
            if (!(fds[i].revents & POLLIN))
                continue;

            // Check whether this FD is one of our listening sockets
            bool isListeningSocket = false;
            size_t serverIndex = 0;

            for (size_t j = 0; j < hosting.size(); ++j)
            {
                if (fds[i].fd == hosting[j].serverFD)
                {
                    isListeningSocket = true;
                    serverIndex = j;
                    break;
                }
            }

            if (isListeningSocket)
                handleNewConnection(fds[i].fd, hosting[serverIndex].conf, fds);
            else
            {
                // This is a client socket.
                // For now, just demonstrate that data is available.
                char buffer[4096];

                ssize_t bytesRead = recv(fds[i].fd, buffer, sizeof(buffer) - 1, 0);
                if (bytesRead <= 0)
                {
                    if (bytesRead == 0)
                        std::cout << "Client disconnected\n";
                    else
                        std::cerr << "recv() failed\n";
                    close(fds[i].fd);

                    // Remove this client from poll()
                    fds.erase(fds.begin() + i);
                    --i;
                    continue;
                }
                buffer[bytesRead] = '\0';
                std::cout << "Received from client:\n" << buffer << std::endl;
            }
        }
    }
}


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
    runEventLoop(hosting, fds);

    return 0;
}
