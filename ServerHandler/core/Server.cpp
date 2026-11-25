#include "Server.hpp"
#include "ServerInternal.hpp"
#include <netdb.h>

namespace
{
    bool setNonBlocking(int fd)
    {
        return fcntl(fd, F_SETFL, O_NONBLOCK) != -1;
    }
}

void Init(Server& server)
{
    std::signal(SIGPIPE, SIG_IGN);

    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST;
    addrinfo* addresses = NULL;
    int addressStatus = getaddrinfo(server.conf.listenAddress.c_str(), NULL, &hints, &addresses);
    if (addressStatus != 0)
        throw std::runtime_error(std::string("invalid listen address '")
                                 + server.conf.listenAddress + "': "
                                 + gai_strerror(addressStatus));
    if (addresses == NULL)
        throw std::runtime_error("could not resolve listen address");

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr = reinterpret_cast<sockaddr_in*>(addresses->ai_addr)->sin_addr;
    address.sin_port = htons(server.conf.listen);
    freeaddrinfo(addresses);

    server.serverFD = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;

    if (server.serverFD == -1)
        throw std::runtime_error("socket() failed");
    if (!setNonBlocking(server.serverFD))
    {
        close(server.serverFD);
        server.serverFD = -1;
        throw std::runtime_error("fcntl(O_NONBLOCK) failed");
    }
    if (setsockopt(server.serverFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        close(server.serverFD);
        server.serverFD = -1;
        throw std::runtime_error("setsockopt() failed");
    }

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
    std::cout << "Listening on " << server.conf.listenAddress << ':'
              << server.conf.listen << std::endl;
}

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

void handleNewConnection(int serverFD, const ServerConfig& config, std::vector<pollfd>& fds, std::map<int, ServerConfig>& clientConfigs)
{
    sockaddr_in clientAddr;
    pollfd client;

    socklen_t addrlen = sizeof(clientAddr);

    int clientFD = accept(serverFD,reinterpret_cast<sockaddr*>(&clientAddr),&addrlen);
    if (clientFD == -1)
    {
        std::cerr << "accept() failed\n";
        return;
    }
    if (!setNonBlocking(clientFD))
    {
        close(clientFD);
        std::cerr << "fcntl(O_NONBLOCK) failed for client\n";
        return;
    }
    client.fd = clientFD;
    client.events = POLLIN;
    client.revents = 0;
    clientConfigs[clientFD] = config;
    fds.push_back(client);
    ServerInternal::registerClient(clientFD);
}
