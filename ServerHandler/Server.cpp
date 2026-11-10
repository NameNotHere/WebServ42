#include "Server.hpp"
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
    void drainCgiInput(CgiHandler& cgi, CgiJob& job)
    {
        cgi.writeInput(job);
    }

    void drainCgiOutput(CgiHandler& cgi, CgiJob& job)
    {
        cgi.readOutput(job);
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

namespace
{
    void updateClientPollEvents(
        const std::vector<Server>& hosting, std::vector<pollfd>& fds,
        std::map<int, std::string>& reqs,
        std::map<int, ServerConfig>& clientConfigs, CgiHandler& cgi,
        std::map<int, CgiJob>& cgiJobs)
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
    }

    void appendClientPollSources(
        const std::vector<Server>& hosting, const std::vector<pollfd>& fds,
        std::vector<pollfd>& pollFds, std::vector<PollSource>& sources)
    {
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
    }

    void appendCgiPollSources(
        const std::map<int, CgiJob>& cgiJobs,
        std::vector<pollfd>& pollFds, std::vector<PollSource>& sources)
    {
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
    }

    void handleListenerEvent(
        const PollSource& source, short events,
        std::vector<Server>& hosting, std::vector<pollfd>& fds,
        std::map<int, ServerConfig>& clientConfigs)
    {
        if (!(events & POLLIN))
            return;
        for (size_t k = 0; k < hosting.size(); ++k)
            if (source.fd == hosting[k].serverFD)
            {
                handleNewConnection(source.fd, hosting[k].conf,
                                    fds, clientConfigs);
                break;
            }
    }

    void handleCgiPipeEvent(
        const PollSource& source, short events, CgiHandler& cgi,
        std::map<int, CgiJob>& cgiJobs)
    {
        std::map<int, CgiJob>::iterator job =
            cgiJobs.find(source.clientFd);
        if (job == cgiJobs.end())
            return;
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
    }

    bool clientIsConnected(int fd, const std::vector<pollfd>& fds)
    {
        for (size_t i = 0; i < fds.size(); ++i)
            if (fds[i].fd == fd)
                return true;
        return false;
    }

    void handleClientEvent(
        int fd, short events, std::vector<pollfd>& fds,
        std::map<int, std::string>& reqs,
        std::map<int, ServerConfig>& clientConfigs, CgiHandler& cgi,
        std::map<int, CgiJob>& cgiJobs)
    {
        if (!clientIsConnected(fd, fds))
            return;
        if (events & (POLLNVAL | POLLERR))
        {
            removeClient(fd, fds, reqs, clientConfigs, cgi, cgiJobs);
            return;
        }
        if (events & POLLOUT)
        {
            if (!flushPendingOutput(fd))
            {
                removeClient(fd, fds, reqs, clientConfigs, cgi, cgiJobs);
                return;
            }
            if (!hasPendingOutput(fd) && shouldCloseAfterOutput(fd))
            {
                removeClient(fd, fds, reqs, clientConfigs, cgi, cgiJobs);
                return;
            }
        }
        if (!(events & POLLIN))
        {
            if (events & POLLHUP)
                removeClient(fd, fds, reqs, clientConfigs, cgi, cgiJobs);
            return;
        }
        if (!largePostClients.count(fd)
            && largePostClients.size() >= MAX_BUFFERED_LARGE_POSTS)
            return;

        char buffer[262144];
        ssize_t bytesRead = recv(fd, buffer, sizeof(buffer), 0);
        if (bytesRead < 0)
        {
            removeClient(fd, fds, reqs, clientConfigs, cgi, cgiJobs);
            return;
        }

        bool peerClosed = (bytesRead == 0);

        if (bytesRead > 0)
        {
            reqs[fd].append(buffer, static_cast<size_t>(bytesRead));
            runHttpParser(fd, reqs, clientConfigs, cgi, cgiJobs);
        }

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
        }
    }

    void handlePollEvent(
        const PollSource& source, short events,
        std::vector<Server>& hosting, std::vector<pollfd>& fds,
        std::map<int, std::string>& reqs,
        std::map<int, ServerConfig>& clientConfigs, CgiHandler& cgi,
        std::map<int, CgiJob>& cgiJobs)
    {
        if (source.kind == POLL_LISTENER)
        {
            handleListenerEvent(source, events, hosting, fds, clientConfigs);
            return;
        }
        if (source.kind == POLL_CGI_INPUT || source.kind == POLL_CGI_OUTPUT)
        {
            handleCgiPipeEvent(source, events, cgi, cgiJobs);
            return;
        }
        handleClientEvent(source.clientFd, events, fds, reqs, clientConfigs,
                          cgi, cgiJobs);
    }

    void completeCgiJobs(
        CgiHandler& cgi, std::map<int, CgiJob>& cgiJobs,
        std::map<int, std::string>& reqs,
        std::map<int, ServerConfig>& clientConfigs,
        std::vector<pollfd>& fds)
    {
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
            std::map<int, ServerConfig>::const_iterator config =
                clientConfigs.find(clientFd);
            queueCgiResponse(clientFd, job->second, result,
                             config == clientConfigs.end() ? NULL
                                                           : &config->second);
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
                    removeClient(clientFd, fds, reqs, clientConfigs,
                                 cgi, cgiJobs);
            }
        }
    }
}

void runEventLoop(std::vector<Server>& hosting, std::vector<pollfd>& fds,
                  std::map<int, std::string>& reqs)
{
    std::map<int, ServerConfig> clientConfigs;
    std::map<int, CgiJob> cgiJobs;
    CgiHandler cgi("/usr/bin/python3", 5);

    while (true)
    {
        updateClientPollEvents(hosting, fds, reqs, clientConfigs, cgi, cgiJobs);

        std::vector<pollfd> pollFds;
        std::vector<PollSource> sources;
        appendClientPollSources(hosting, fds, pollFds, sources);
        appendCgiPollSources(cgiJobs, pollFds, sources);

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
            handlePollEvent(sources[j], events, hosting, fds, reqs,
                            clientConfigs, cgi, cgiJobs);
        }

        completeCgiJobs(cgi, cgiJobs, reqs, clientConfigs, fds);
    }
}

namespace
{
    void rejectBareLfRequest(int fd, std::map<int, std::string>& reqs,
                             std::map<int, ServerConfig>& clientConfigs)
    {
        const std::string& request = reqs[fd];
        for (std::string::size_type i = 0; i < request.size(); ++i)
        {
            if (request[i] == '\n'
                && (i == 0 || request[i - 1] != '\r'))
            {
                std::map<int, ServerConfig>::iterator config =
                    clientConfigs.find(fd);
                sendErrorResponse(fd, "HTTP/1.1", false, 400,
                                  config == clientConfigs.end() ? NULL
                                                                : &config->second);
                closeAfterPendingOutput(fd);
                reqs.erase(fd);
                clientConfigs.erase(fd);
                return;
            }
        }
    }

    size_t requestMaxBody(int fd, const std::string& rawRequest,
                          std::map<int, ServerConfig>& clientConfigs)
    {
        size_t maxBody = 0;
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
        return maxBody;
    }

    bool processValidRequest(
        int fd, HttpParser& http, bool& keepAlive,
        std::map<int, std::string>& reqs,
        std::map<int, ServerConfig>& clientConfigs, CgiHandler& cgi,
        std::map<int, CgiJob>& cgiJobs)
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
            return false;
        }
        if (!keepAlive)
        {
            closeAfterPendingOutput(fd);
            reqs.erase(fd);
            clientConfigs.erase(fd);
            return false;
        }
        return !reqs[fd].empty();
    }

    void sendParserErrorResponse(
        int fd, HttpParser::RequestStatus status,
        std::map<int, std::string>& reqs,
        std::map<int, ServerConfig>& clientConfigs)
    {
        int code = 400;
        if (status == HttpParser::REQUEST_METHOD_NOT_ALLOWED) code = 405;
        // else if (status == HttpParser::REQUEST_METHOD_NOT_IMPLEMENTED) code = 501;
        else if (status == HttpParser::REQUEST_METHOD_NOT_IMPLEMENTED) code = 405;
        else if (status == HttpParser::REQUEST_TARGET_NOT_FOUND) code = 404;
        else if (status == HttpParser::REQUEST_VERSION_NOT_SUPPORTED) code = 505;
        else if (status == HttpParser::REQUEST_CONTENT_TOO_LARGE) code = 413;
        std::map<int, ServerConfig>::iterator config =
            clientConfigs.find(fd);
        std::vector<std::pair<std::string, std::string> > extraHeaders;
        if (code == 405 && config != clientConfigs.end())
        {
            extraHeaders.push_back(std::make_pair(
                "Allow", allowHeaderValue(config->second, NULL)));
        }
        sendErrorResponse(fd, "HTTP/1.1", false, code,
                          config == clientConfigs.end() ? NULL
                                                        : &config->second,
                          extraHeaders);
        closeAfterPendingOutput(fd);
        reqs.erase(fd);
        clientConfigs.erase(fd);
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
            rejectBareLfRequest(fd, reqs, clientConfigs);
            return;
        }

        size_t maxBody = requestMaxBody(fd, reqs[fd], clientConfigs);
        HttpParser::RequestStatus status =
            http.parseHttpRequest(reqs[fd], maxBody);
        if (status == HttpParser::REQUEST_VALID)
        {
            if (!processValidRequest(fd, http, keepAlive, reqs,
                                     clientConfigs, cgi, cgiJobs))
                return;
            continue;
        }
        else if (status == HttpParser::REQUEST_INCOMPLETE)
        {
            if (isLargePost(http))
                largePostClients.insert(fd);
            return;
        }
        else
        {
            sendParserErrorResponse(fd, status, reqs, clientConfigs);
            return;
        }
    }
}
