#include "Server.hpp"
#include <set>
#include <netdb.h>

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
    const size_t MAX_BUFFERED_LARGE_POSTS = 5;
    std::set<int> largePostClients;
    std::set<int> largePostWaiters;
    std::set<int> peerClosedClients;

    void promoteLargePostWaiters(
        const std::vector<pollfd>& fds,
        const std::map<int, std::string>& reqs)
    {
        while (largePostClients.size() < MAX_BUFFERED_LARGE_POSTS
               && !largePostWaiters.empty())
        {
            int fd = *largePostWaiters.begin();
            largePostWaiters.erase(largePostWaiters.begin());
            bool connected = false;
            for (size_t i = 0; i < fds.size(); ++i)
                if (fds[i].fd == fd)
                    connected = true;
            if (connected && reqs.find(fd) != reqs.end())
                largePostClients.insert(fd);
        }
    }
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
        return fcntl(fd, F_SETFL, O_NONBLOCK) != -1;
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
        largePostWaiters.erase(fd);
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

    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST;
    addrinfo* addresses = NULL;
    int addressStatus = getaddrinfo(server.conf.listenAddress.c_str(), NULL,
                                    &hints, &addresses);
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
    const unsigned char* octets = reinterpret_cast<const unsigned char*>(&clientAddr.sin_addr);
    std::ostringstream ip;

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
    ip << static_cast<unsigned int>(octets[0]) << "."
       << static_cast<unsigned int>(octets[1]) << "."
       << static_cast<unsigned int>(octets[2]) << "."
       << static_cast<unsigned int>(octets[3]);

    client.fd = clientFD;
    client.events = POLLIN;
    client.revents = 0;
    clientConfigs[clientFD] = config;
    fds.push_back(client);
    std::cout << "Client connected from " << ip.str() << ":" << ntohs(clientAddr.sin_port) << " to port " << config.listen << std::endl;
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
                && !largePostWaiters.count(fd))
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
        if (largePostWaiters.count(fd))
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
            const ServerConfig* serverConfig = NULL;
            if (config != clientConfigs.end())
                serverConfig = &config->second;
            queueCgiResponse(clientFd, job->second, result, serverConfig);
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
        promoteLargePostWaiters(fds, reqs);
        updateClientPollEvents(hosting, fds, reqs, clientConfigs, cgi, cgiJobs);

        std::vector<pollfd> pollFds;
        std::vector<PollSource> sources;
        appendClientPollSources(hosting, fds, pollFds, sources);
        appendCgiPollSources(cgiJobs, pollFds, sources);

        int timeout = 100;
        if (cgiJobs.empty())
            timeout = -1;
        int ready = poll(pollFds.data(), pollFds.size(), timeout);
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
                const ServerConfig* serverConfig = NULL;
                if (config != clientConfigs.end())
                    serverConfig = &config->second;
                sendErrorResponse(fd, "HTTP/1.1", false, 400, serverConfig);
                closeAfterPendingOutput(fd);
                reqs.erase(fd);
                clientConfigs.erase(fd);
                largePostClients.erase(fd);
                largePostWaiters.erase(fd);
                return;
            }
        }
    }

    size_t requestMaxBody(int fd, const std::string& rawRequest,
                          std::map<int, ServerConfig>& clientConfigs)
    {
        size_t maxBody = 0;
        size_t firstSpace = rawRequest.find(' ');
        size_t secondSpace = std::string::npos;
        if (firstSpace != std::string::npos)
            secondSpace = rawRequest.find(' ', firstSpace + 1);

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
        bool responseBuilt = buildResponse(http, fd, keepAlive, clientConfigs.at(fd), cgi, cgiJobs);
        reqs[fd].erase(0, requestLen);
        if (cgiJobs.find(fd) == cgiJobs.end())
            largePostClients.erase(fd);

        largePostWaiters.erase(fd);
        if (cgiJobs.find(fd) != cgiJobs.end())
        {
            if (reqs[fd].empty())
                std::string().swap(reqs[fd]);
            return false;
        }
        if (!responseBuilt)
        {
            closeAfterPendingOutput(fd);
            reqs.erase(fd);
            clientConfigs.erase(fd);
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
        else if (status == HttpParser::REQUEST_METHOD_NOT_IMPLEMENTED) code = 501;
        else if (status == HttpParser::REQUEST_TARGET_NOT_FOUND) code = 404;
        else if (status == HttpParser::REQUEST_VERSION_NOT_SUPPORTED) code = 505;
        else if (status == HttpParser::REQUEST_CONTENT_TOO_LARGE) code = 413;
        std::map<int, ServerConfig>::iterator config =
            clientConfigs.find(fd);
        std::vector<std::pair<std::string, std::string> > extraHeaders;
        if (code == 405 && config != clientConfigs.end())
        {
            const LocationConfig* location = NULL;
            std::map<int, std::string>::const_iterator request = reqs.find(fd);
            if (request != reqs.end())
            {
                size_t firstSpace = request->second.find(' ');
                if (firstSpace != std::string::npos)
                {
                    size_t secondSpace = request->second.find(' ', firstSpace + 1);
                    if (secondSpace != std::string::npos)
                    {
                        std::string target = request->second.substr(
                            firstSpace + 1, secondSpace - firstSpace - 1);
                        location = findMatchingLocation(
                            config->second, target, "HEAD");
                    }
                }
            }
            extraHeaders.push_back(std::make_pair(
                "Allow", allowHeaderValue(config->second, location)));
        }
        const ServerConfig* serverConfig = NULL;
        if (config != clientConfigs.end())
            serverConfig = &config->second;
        sendErrorResponse(fd, "HTTP/1.1", false, code, serverConfig,
                          extraHeaders);
        closeAfterPendingOutput(fd);
        reqs.erase(fd);
        clientConfigs.erase(fd);
        largePostClients.erase(fd);
        largePostWaiters.erase(fd);
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
        HttpParser::RequestStatus status = http.parseHttpRequest(reqs[fd], maxBody);
        if (status == HttpParser::REQUEST_VALID)
        {
            if (!processValidRequest(fd, http, keepAlive, reqs, clientConfigs, cgi, cgiJobs))
                return;
            continue;
        }
        else if (status == HttpParser::REQUEST_INCOMPLETE)
        {
            if (isLargePost(http))
            {
                if (largePostClients.count(fd))
                    return;
                if (largePostClients.size() < MAX_BUFFERED_LARGE_POSTS)
                    largePostClients.insert(fd);
                else
                    largePostWaiters.insert(fd);
            }
            return;
        }
        else
        {
            sendParserErrorResponse(fd, status, reqs, clientConfigs);
            largePostClients.erase(fd);
            largePostWaiters.erase(fd);
            return;
        }
    }
}
