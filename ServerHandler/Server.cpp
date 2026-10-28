#include "Server.hpp"

namespace
{
    enum PollKind
    {
        POLL_LISTENER,
        POLL_CLIENT,
        POLL_CGI_INPUT,
        POLL_CGI_OUTPUT
    };

    struct PollSource
    {
        int kind;
        int fd;
        int clientFd;
    };

    bool setNonBlocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        return flags != -1 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
    }

    void removeClient(int fd, std::vector<pollfd>& fds,
                      std::map<int, std::string>& requests,
                      std::map<int, ServerConfig>& configs,
                      CgiHandler& cgi, std::map<int, CgiJob>& cgiJobs)
    {
        std::map<int, CgiJob>::iterator job = cgiJobs.find(fd);
        if (job != cgiJobs.end())
        {
            cgi.cancel(job->second);
            cgiJobs.erase(job);
        }
        close(fd);
        clearPendingOutput(fd);
        requests.erase(fd);
        configs.erase(fd);
        for (size_t i = 0; i < fds.size(); ++i)
        {
            if (fds[i].fd == fd)
            {
                fds.erase(fds.begin() + i);
                break;
            }
        }
    }
}

void Init(Server& server)
{
    std::signal(SIGPIPE, SIG_IGN);
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
    char ip[INET_ADDRSTRLEN];

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
    if (inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip)) == NULL)
    {
        std::cerr << "inet_ntop() failed\n";
        close(clientFD);
        return;
    }

    client.fd = clientFD;
    client.events = POLLIN;
    client.revents = 0;
    clientConfigs[clientFD] = config;
    fds.push_back(client);
    std::cout << "Client connected from " << ip << ":" << ntohs(clientAddr.sin_port) << " to port " << config.listen << std::endl;
}

void runEventLoop(std::vector<Server>& hosting, std::vector<pollfd>& fds,
                  std::map<int, std::string>& reqs)
{
    std::map<int, ServerConfig> clientConfigs;
    std::map<int, CgiJob> cgiJobs;
    CgiHandler cgi("/usr/bin/python3", 5);

    while (true)
    {
        size_t i = 0;
        while (i < fds.size())
        {
            bool listener = false;
            for (size_t j = 0; j < hosting.size(); ++j)
                if (fds[i].fd == hosting[j].serverFD)
                    listener = true;
            if (listener)
            {
                fds[i].events = POLLIN;
                ++i;
                continue;
            }
            int fd = fds[i].fd;
            fds[i].events = 0;
            if (cgiJobs.find(fd) == cgiJobs.end()
                && !shouldCloseAfterOutput(fd))
                fds[i].events |= POLLIN;
            if (hasPendingOutput(fd))
                fds[i].events |= POLLOUT;
            if (shouldCloseAfterOutput(fd) && !hasPendingOutput(fd))
                removeClient(fd, fds, reqs, clientConfigs, cgi, cgiJobs);
            else
                ++i;
        }

        std::vector<pollfd> pollFds;
        std::vector<PollSource> sources;
        for (size_t j = 0; j < fds.size(); ++j)
        {
            pollFds.push_back(fds[j]);
            PollSource source;
            source.fd = fds[j].fd;
            source.clientFd = fds[j].fd;
            source.kind = POLL_CLIENT;
            for (size_t k = 0; k < hosting.size(); ++k)
            {
                if (fds[j].fd == hosting[k].serverFD)
                {
                    source.kind = POLL_LISTENER;
                    source.clientFd = -1;
                    break;
                }
            }
            sources.push_back(source);
        }
        for (std::map<int, CgiJob>::const_iterator it = cgiJobs.begin();
             it != cgiJobs.end(); ++it)
        {
            if (it->second.stdinFd != -1)
            {
                pollfd pipeFd;
                pipeFd.fd = it->second.stdinFd;
                pipeFd.events = POLLOUT;
                pipeFd.revents = 0;
                pollFds.push_back(pipeFd);
                PollSource source = { POLL_CGI_INPUT, pipeFd.fd, it->first };
                sources.push_back(source);
            }
            if (it->second.stdoutFd != -1)
            {
                pollfd pipeFd;
                pipeFd.fd = it->second.stdoutFd;
                pipeFd.events = POLLIN;
                pipeFd.revents = 0;
                pollFds.push_back(pipeFd);
                PollSource source = { POLL_CGI_OUTPUT, pipeFd.fd, it->first };
                sources.push_back(source);
            }
        }

        int ready = poll(pollFds.data(), pollFds.size(),
                         cgiJobs.empty() ? -1 : 100);
        if (ready == -1)
        {
            std::cerr << "poll() failed\n";
            break;
        }

        for (size_t j = 0; j < pollFds.size(); ++j)
        {
            short events = pollFds[j].revents;
            if (events == 0)
                continue;
            const PollSource& source = sources[j];

            if (source.kind == POLL_LISTENER)
            {
                if (events & POLLIN)
                    for (size_t k = 0; k < hosting.size(); ++k)
                        if (source.fd == hosting[k].serverFD)
                        {
                            handleNewConnection(source.fd, hosting[k].conf,
                                                fds, clientConfigs);
                            break;
                        }
                continue;
            }
            if (source.kind == POLL_CGI_INPUT
                || source.kind == POLL_CGI_OUTPUT)
            {
                std::map<int, CgiJob>::iterator job =
                    cgiJobs.find(source.clientFd);
                if (job == cgiJobs.end())
                    continue;
                bool inputPipe = source.kind == POLL_CGI_INPUT;
                if (events & POLLNVAL)
                    cgi.pipeError(job->second, inputPipe);
                else if (inputPipe)
                {
                    if (events & (POLLERR | POLLHUP))
                        cgi.pipeError(job->second, true);
                    else if (events & POLLOUT)
                        cgi.writeInput(job->second);
                }
                else
                {
                    if (events & (POLLIN | POLLHUP))
                        cgi.readOutput(job->second);
                    if ((events & POLLERR) && job->second.stdoutFd != -1)
                        cgi.pipeError(job->second, false);
                }
                continue;
            }

            int fd = source.clientFd;
            bool connected = false;
            for (size_t k = 0; k < fds.size(); ++k)
                if (fds[k].fd == fd)
                    connected = true;
            if (!connected)
                continue;
            if (events & (POLLNVAL | POLLERR))
            {
                removeClient(fd, fds, reqs, clientConfigs, cgi, cgiJobs);
                continue;
            }
            if (events & POLLOUT)
            {
                if (!flushPendingOutput(fd))
                {
                    removeClient(fd, fds, reqs, clientConfigs, cgi, cgiJobs);
                    continue;
                }
                if (!hasPendingOutput(fd) && shouldCloseAfterOutput(fd))
                {
                    removeClient(fd, fds, reqs, clientConfigs, cgi, cgiJobs);
                    continue;
                }
            }
            if (!(events & POLLIN))
            {
                if (events & POLLHUP)
                    removeClient(fd, fds, reqs, clientConfigs, cgi, cgiJobs);
                continue;
            }
            char buffer[4096];
            ssize_t bytesRead = recv(fd, buffer, sizeof(buffer), 0);
            if (bytesRead == 0)
            {
                std::cout << "Client disconnected\n";
                removeClient(fd, fds, reqs, clientConfigs, cgi, cgiJobs);
                continue;
            }
            if (bytesRead < 0)
                continue;
            reqs[fd].append(buffer, static_cast<size_t>(bytesRead));
            runHttpParser(fd, reqs, clientConfigs, cgi, cgiJobs);
        }

        std::vector<int> completedClients;
        for (std::map<int, CgiJob>::iterator it = cgiJobs.begin();
             it != cgiJobs.end(); ++it)
        {
            cgi.update(it->second);
            if (cgi.complete(it->second))
                completedClients.push_back(it->first);
        }
        for (size_t j = 0; j < completedClients.size(); ++j)
        {
            int clientFd = completedClients[j];
            std::map<int, CgiJob>::iterator job = cgiJobs.find(clientFd);
            if (job == cgiJobs.end())
                continue;
            CGIResult result = cgi.result(job->second);
            queueCgiResponse(clientFd, job->second, result);
            bool keepAlive = job->second.keepAlive;
            cgi.cancel(job->second);
            cgiJobs.erase(job);
            if (!keepAlive)
                closeAfterPendingOutput(clientFd);
            else if (reqs.find(clientFd) != reqs.end()
                     && !reqs[clientFd].empty())
                runHttpParser(clientFd, reqs, clientConfigs, cgi, cgiJobs);
        }
    }
}

void runHttpParser(int fd, std::map<int, std::string>& reqs,
                   std::map<int, ServerConfig>& clientConfigs, CgiHandler& cgi,
                   std::map<int, CgiJob>& cgiJobs)
{
    bool keepAlive = true;
    HttpParser http;
    while (true)
    {
        if (reqs[fd].find("\r\n\r\n") == std::string::npos)
        {
            std::cout << "Request:" << reqs[fd] << "\n";
            const std::string& request = reqs[fd];

            for (std::string::size_type i = 0; i < request.size(); ++i)
            {
                if (request[i] == '\n'
                    && (i == 0 || request[i - 1] != '\r'))
                {
                    std::string response = Response::create(400, "");
                    sendAll(fd, response.data(), response.size());
                    closeAfterPendingOutput(fd);
                    reqs.erase(fd);
                    clientConfigs.erase(fd);
                    return;
                }
            }
            std::cout << "REQUEST INCOMPLETE\n";
            return;
        }

        size_t maxBody = 0;
        const std::string& rawRequest = reqs[fd];
        size_t firstSpace = rawRequest.find(' ');
        size_t secondSpace = firstSpace == std::string::npos
            ? std::string::npos
            : rawRequest.find(' ', firstSpace + 1);

        if (firstSpace != std::string::npos
            && secondSpace != std::string::npos)
        {
            std::string target = rawRequest.substr(
                firstSpace + 1, secondSpace - firstSpace - 1);

            const LocationConfig* location =
                findMatchingLocation(clientConfigs.at(fd), target);

            if (location != NULL && location->hasMaxBody)
                maxBody = location->maxBody;
        }

        HttpParser::RequestStatus status =
            http.parseHttpRequest(reqs[fd], maxBody);
        if (status == HttpParser::REQUEST_VALID)
        {
            size_t requestLen = http.getRequestLength();
            buildResponse(http, fd, keepAlive, clientConfigs.at(fd), cgi, cgiJobs);
            reqs[fd].erase(0, requestLen);
            if (cgiJobs.find(fd) != cgiJobs.end())
                return;
            if (!keepAlive)
            {
                closeAfterPendingOutput(fd);
                reqs.erase(fd);
                clientConfigs.erase(fd);
                return;
            }
            if (reqs[fd].empty())
                return;
        }
        else if (status == HttpParser::REQUEST_INCOMPLETE)
            return;
        else
        {
            int code = 400;
            if (status == HttpParser::REQUEST_METHOD_NOT_ALLOWED) code = 405;
            else if (status == HttpParser::REQUEST_METHOD_NOT_IMPLEMENTED) code = 405;
            // else if (status == HttpParser::REQUEST_METHOD_NOT_IMPLEMENTED) code = 501;
            else if (status == HttpParser::REQUEST_TARGET_NOT_FOUND) code = 404;
            else if (status == HttpParser::REQUEST_VERSION_NOT_SUPPORTED) code = 505;
            else if (status == HttpParser::REQUEST_CONTENT_TOO_LARGE) code = 413;
            std::string response = Response::create(code, "");
            sendAll(fd, response.data(), response.size());
            closeAfterPendingOutput(fd);
            reqs.erase(fd);
            clientConfigs.erase(fd);
            return;
        }
    }
}
