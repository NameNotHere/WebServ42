#include "Server.hpp"

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

void runEventLoop(std::vector<Server>& hosting, std::vector<pollfd>& fds, std::map<int, std::string> &reqs)
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
                int fd = fds[i].fd;
                HttpParser http;
                char buffer[4096];
                ssize_t bytesRead = recv(fd, buffer, sizeof(buffer), 0);

                if (bytesRead <= 0)
                {
                    if (bytesRead == 0)
                        std::cout << "Client disconnected\n";
                    else
                        std::cerr << "recv() failed\n";
                    close(fd);
                    reqs.erase(fd);
                    fds.erase(fds.begin() + i);
                    i--;
                    continue;
                }
                reqs[fd].append(buffer, bytesRead);
                // This gets ignored if theres only \r\n\r but it should error or somthing 
                if (reqs[fd].find("\r\n\r\n") == std::string::npos)
                {
                    std::cout << "REQUEST INCOMPLETE\n";
                    continue;
                }    
                HttpParser::RequestStatus status = http.parseHttpRequest(reqs[fd]);

                if(status == HttpParser::REQUEST_VALID)
                {
                    std::cout << "REQUEST COMPLETE!\n";
                    size_t requestLen = http.getRequestLength();
                    std::cout << "Request Length:" << requestLen << "\n";
                    std::cout << "Debug-accum request:\n" << reqs[fd] << "\n";

                    reqs[fd].erase(0, requestLen);
                }
                else if(status == HttpParser::REQUEST_INCOMPLETE)
                {
                    std::cout << "REQUEST INCOMPLETE, WAITING FOR MORE DATA!\n";
                }
                else if(status == HttpParser::REQUEST_INVALID)
                {
                    std::cout << "REQUEST INVALID!\n";
                    std::string response =
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n"
                    "\r\n";

                    send(fd, response.c_str(), response.size(), 0);

                    close(fd);
                    reqs.erase(fd);
                    fds.erase(fds.begin() + i);
                    i--;
                    std::cout << "Client disconnected\n";
                    continue;
                }
                std::cout << "Accumulated request:\n" << reqs[fd] << "\n";
            }
        }
    }
}