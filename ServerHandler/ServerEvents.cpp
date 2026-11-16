#include "Server.hpp"
#include "ServerInternal.hpp"
#include <set>
#include <netdb.h>

namespace ServerInternal
{
    volatile sig_atomic_t shutdownRequested = 0;
    std::set<int> largePostClients;
    std::set<int> largePostWaiters;
    std::set<int> peerClosedClients;

    void handleShutdownSignal(int)
    {
        shutdownRequested = 1;
    }

    void installShutdownHandlers()
    {
        shutdownRequested = 0;
        if (std::signal(SIGINT, handleShutdownSignal) == SIG_ERR
            || std::signal(SIGTERM, handleShutdownSignal) == SIG_ERR)
            throw std::runtime_error("could not install shutdown signal handlers");
    }
}

using namespace ServerInternal;

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

    void promoteLargePostWaiters(const std::vector<pollfd>& fds, const std::map<int, std::string>& reqs)
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

    bool isListener(int fd, const std::vector<Server>& hosting)
    {
        for (size_t i = 0; i < hosting.size(); ++i)
            if (hosting[i].serverFD == fd)
                return true;
        return false;
    }

    void cleanupEventLoop(
        const std::vector<Server>& hosting, std::vector<pollfd>& fds,
        std::map<int, std::string>& requests,
        std::map<int, ServerConfig>& configs, CgiHandler& cgi,
        std::map<int, CgiJob>& cgiJobs)
    {
        size_t i = 0;
        while (i < fds.size())
        {
            int fd = fds[i].fd;
            if (isListener(fd, hosting))
            {
                ++i;
                continue;
            }
            removeClient(fd, fds, requests, configs, cgi, cgiJobs);
        }
        for (std::map<int, CgiJob>::iterator it = cgiJobs.begin();
             it != cgiJobs.end(); ++it)
            cgi.cancel(it->second);
        cgiJobs.clear();
        requests.clear();
        configs.clear();
        largePostClients.clear();
        largePostWaiters.clear();
        peerClosedClients.clear();
    }
}

namespace
{
    void updateClientPollEvents(const std::vector<Server>& hosting, std::vector<pollfd>& fds,
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

    void appendClientPollSources(const std::vector<Server>& hosting, const std::vector<pollfd>& fds,
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

    void appendCgiPollSources(const std::map<int, CgiJob>& cgiJobs,
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

    void handleListenerEvent(const PollSource& source, short events,
                             std::vector<Server>& hosting, std::vector<pollfd>& fds,
                             std::map<int, ServerConfig>& clientConfigs)
    {
        if (!(events & POLLIN))
            return;
        for (size_t k = 0; k < hosting.size(); ++k)
            if (source.fd == hosting[k].serverFD)
            {
                handleNewConnection(source.fd, hosting[k].conf, fds, clientConfigs);
                break;
            }
    }

    void handleCgiPipeEvent(const PollSource& source, short events, CgiHandler& cgi,
                            std::map<int, CgiJob>& cgiJobs)
    {
        std::map<int, CgiJob>::iterator job = cgiJobs.find(source.clientFd);
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

    void handleClientEvent(int fd, short events, std::vector<pollfd>& fds,
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

    void handlePollEvent(const PollSource& source, short events,
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

    void completeCgiJobs(CgiHandler& cgi, std::map<int, CgiJob>& cgiJobs,
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
            std::map<int, ServerConfig>::const_iterator config = clientConfigs.find(clientFd);
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
            else if (reqs.find(clientFd) != reqs.end() && !reqs[clientFd].empty())
                runHttpParser(clientFd, reqs, clientConfigs, cgi, cgiJobs);
            if (peerClosedClients.count(clientFd) && cgiJobs.find(clientFd) == cgiJobs.end())
            {
                if (hasPendingOutput(clientFd))
                    closeAfterPendingOutput(clientFd);
                else
                    removeClient(clientFd, fds, reqs, clientConfigs, cgi, cgiJobs);
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

    while (!shutdownRequested)
    {
        promoteLargePostWaiters(fds, reqs);
        updateClientPollEvents(hosting, fds, reqs, clientConfigs, cgi, cgiJobs);

        std::vector<pollfd> pollFds;
        std::vector<PollSource> sources;
        appendClientPollSources(hosting, fds, pollFds, sources);
        appendCgiPollSources(cgiJobs, pollFds, sources);

        int timeout = 100;
        int ready = poll(pollFds.data(), pollFds.size(), timeout);
        if (ready == -1)
        {
            if (shutdownRequested)
                break;
            std::cerr << "poll() failed\n";
            break;
        }
        if (shutdownRequested)
            break;
        for (size_t j = 0; j < pollFds.size(); ++j)
        {
            short events = pollFds[j].revents;
            if (events == 0)
                continue;
            handlePollEvent(sources[j], events, hosting, fds, reqs, clientConfigs, cgi, cgiJobs);
        }
        completeCgiJobs(cgi, cgiJobs, reqs, clientConfigs, fds);
    }
    cleanupEventLoop(hosting, fds, reqs, clientConfigs, cgi, cgiJobs);
}
