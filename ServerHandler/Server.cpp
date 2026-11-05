#include "Server.hpp"
#include <cerrno>
#include <set>

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

    const size_t LARGE_POST_THRESHOLD = 8 * 1024 * 1024;
    // Large CGI requests and responses are buffered in memory. Limit the
    // number active at once to cap memory use while allowing concurrent work.
    const size_t MAX_BUFFERED_LARGE_POSTS = 3;
    std::set<int> largePostClients;
    std::set<int> peerClosedClients;
    const size_t MAX_CGI_PIPE_BYTES_PER_EVENT = 1024 * 1024;

    void drainCgiInput(CgiHandler& cgi, CgiJob& job)
    {
        size_t initialOffset = job.bodyOffset;
        while (job.stdinFd != -1
               && job.bodyOffset - initialOffset < MAX_CGI_PIPE_BYTES_PER_EVENT)
        {
            size_t previousOffset = job.bodyOffset;
            cgi.writeInput(job);
            if (job.bodyOffset == previousOffset)
                break;
        }
    }

    void drainCgiOutput(CgiHandler& cgi, CgiJob& job)
    {
        size_t initialSize = job.output.size();
        while (job.stdoutFd != -1
               && job.output.size() - initialSize < MAX_CGI_PIPE_BYTES_PER_EVENT)
        {
            size_t previousSize = job.output.size();
            cgi.readOutput(job);
            if (job.output.size() == previousSize)
                break;
        }
    }

    bool isLargePost(const HttpParser& http)
    {
        if (http.getMethod() != "POST")
            return false;
        const std::map<std::string, std::string>& headers = http.getHeaders();
        if (headers.find("transfer-encoding") != headers.end())
            return true;
        std::map<std::string, std::string>::const_iterator it = headers.find("content-length");
        if (it == headers.end())
            return false;
        try { return std::stoull(it->second) > LARGE_POST_THRESHOLD; }
        catch (const std::exception&) { return false; }
    }

    bool setNonBlocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        return flags != -1 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
    }

    void enlargeCgiInputPipe(int fd)
    {
#ifdef F_SETPIPE_SZ
        const int sizes[] = { 1024 * 1024, 512 * 1024, 256 * 1024,
                              128 * 1024, 64 * 1024 };
        for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i)
            if (fcntl(fd, F_SETPIPE_SZ, sizes[i]) != -1)
                return;
#endif
        (void)fd;
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
        largePostClients.erase(fd);
        peerClosedClients.erase(fd);
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
                && !shouldCloseAfterOutput(fd)
                && (largePostClients.count(fd)
                    || largePostClients.size() < MAX_BUFFERED_LARGE_POSTS))
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
                        drainCgiInput(cgi, job->second);
                }
                else
                {
                    if (events & (POLLIN | POLLHUP))
                        drainCgiOutput(cgi, job->second);
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
            if (!largePostClients.count(fd)
                && largePostClients.size() >= MAX_BUFFERED_LARGE_POSTS)
                continue;
            // Drain available socket data in bounded batches. This reduces
            // poll/parser work for large uploads without letting one client
            // monopolize the event loop.
            char buffer[262144];
            size_t bytesReceived = 0;
            const size_t maxBytesPerEvent = 1024 * 1024;
            bool disconnected = false;
            bool peerClosed = false;
            while (bytesReceived < maxBytesPerEvent)
            {
                size_t capacity = sizeof(buffer);
                if (capacity > maxBytesPerEvent - bytesReceived)
                    capacity = maxBytesPerEvent - bytesReceived;
                ssize_t bytesRead = recv(fd, buffer, capacity, 0);
                if (bytesRead == 0)
                {
                    peerClosed = true;
                    break;
                }
                if (bytesRead < 0)
                {
                    if (errno == EINTR)
                        continue;
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        removeClient(fd, fds, reqs, clientConfigs, cgi, cgiJobs);
                        disconnected = true;
                    }
                    break;
                }
                reqs[fd].append(buffer, static_cast<size_t>(bytesRead));
                bytesReceived += static_cast<size_t>(bytesRead);
            }
            if (disconnected)
                continue;
            if (bytesReceived != 0)
                runHttpParser(fd, reqs, clientConfigs, cgi, cgiJobs);
            if (peerClosed)
            {
                peerClosedClients.insert(fd);
                if (cgiJobs.find(fd) == cgiJobs.end())
                {
                    if (hasPendingOutput(fd))
                        closeAfterPendingOutput(fd);
                    else
                        removeClient(fd, fds, reqs, clientConfigs, cgi, cgiJobs);
                }
                continue;
            }
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
            job->second.requestBody.clear();
            std::string().swap(job->second.requestBody);
            CGIResult result = cgi.result(job->second);
            queueCgiResponse(clientFd, job->second, result);
            bool keepAlive = job->second.keepAlive;
            cgi.cancel(job->second);
            cgiJobs.erase(job);
            largePostClients.erase(clientFd);
            if (!keepAlive)
                closeAfterPendingOutput(clientFd);
            else if (reqs.find(clientFd) != reqs.end()
                     && !reqs[clientFd].empty())
                runHttpParser(clientFd, reqs, clientConfigs, cgi, cgiJobs);
            if (peerClosedClients.count(clientFd)
                && cgiJobs.find(clientFd) == cgiJobs.end())
            {
                if (hasPendingOutput(clientFd))
                    closeAfterPendingOutput(clientFd);
                else
                    removeClient(clientFd, fds, reqs, clientConfigs, cgi, cgiJobs);
            }
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
            std::string method = rawRequest.substr(0, firstSpace);
            std::string target = rawRequest.substr(
                firstSpace + 1, secondSpace - firstSpace - 1);

            const LocationConfig* location =
                findMatchingLocation(clientConfigs.at(fd), target, method);

            if (location != NULL && location->hasMaxBody)
                maxBody = location->maxBody;
        }

        HttpParser::RequestStatus status =
            http.parseHttpRequest(reqs[fd], maxBody);
        if (status == HttpParser::REQUEST_VALID)
        {
            size_t requestLen = http.getRequestLength();
            buildResponse(http, fd, keepAlive, clientConfigs.at(fd), cgi, cgiJobs);
            std::map<int, CgiJob>::iterator cgiJob = cgiJobs.find(fd);
            if (cgiJob != cgiJobs.end() && cgiJob->second.stdinFd != -1)
                enlargeCgiInputPipe(cgiJob->second.stdinFd);
            reqs[fd].erase(0, requestLen);
            if (cgiJobs.find(fd) == cgiJobs.end())
                largePostClients.erase(fd);
            if (cgiJobs.find(fd) != cgiJobs.end())
            {
                if (reqs[fd].empty())
                    std::string().swap(reqs[fd]);
                return;
            }
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
        {
            if (isLargePost(http))
                largePostClients.insert(fd);
            return;
        }
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
