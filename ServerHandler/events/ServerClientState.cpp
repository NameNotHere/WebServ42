#include "../core/Server.hpp"
#include "../core/ServerInternal.hpp"
#include "ServerEventsInternal.hpp"
#include <set>

namespace ServerInternal
{
    volatile sig_atomic_t shutdownRequested = 0;
    std::map<int, ClientTiming> clientTimings;
    std::set<int> largePostClients;
    std::set<int> largePostWaiters;
    std::set<int> peerClosedClients;

    ClientTiming::ClientTiming()
        : lastActivity(std::chrono::steady_clock::now()),
          requestStarted(), requestActive(false)
    {
    }

    void registerClient(int fd)
    {
        clientTimings[fd] = ClientTiming();
    }

    void recordClientRead(int fd, bool startsRequest)
    {
        std::map<int, ClientTiming>::iterator timing = clientTimings.find(fd);
        if (timing == clientTimings.end())
            return;
        const std::chrono::steady_clock::time_point now =
            std::chrono::steady_clock::now();
        timing->second.lastActivity = now;
        if (startsRequest || !timing->second.requestActive)
        {
            timing->second.requestStarted = now;
            timing->second.requestActive = true;
        }
    }

    void recordClientWrite(int fd)
    {
        std::map<int, ClientTiming>::iterator timing = clientTimings.find(fd);
        if (timing != clientTimings.end())
            timing->second.lastActivity = std::chrono::steady_clock::now();
    }

    void completeClientRequest(int fd, bool anotherRequestBuffered)
    {
        std::map<int, ClientTiming>::iterator timing = clientTimings.find(fd);
        if (timing == clientTimings.end())
            return;
        timing->second.requestActive = anotherRequestBuffered;
        if (anotherRequestBuffered)
            timing->second.requestStarted = std::chrono::steady_clock::now();
    }

    void forgetClient(int fd)
    {
        clientTimings.erase(fd);
    }

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

namespace ServerEventsInternal
{
    using namespace ServerInternal;

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
            {
                largePostClients.insert(fd);
                // The request starts consuming an active slot from this point.
                std::map<int, ClientTiming>::iterator timing =
                    clientTimings.find(fd);
                if (timing != clientTimings.end())
                {
                    const std::chrono::steady_clock::time_point now =
                        std::chrono::steady_clock::now();
                    timing->second.lastActivity = now;
                    timing->second.requestStarted = now;
                    timing->second.requestActive = true;
                }
            }
        }
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
        forgetClient(fd);
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

    bool clientHasExpired(int fd,
                          std::chrono::steady_clock::time_point now)
    {
        // These clients are intentionally paused until a large-request slot opens.
        if (largePostWaiters.count(fd))
            return false;
        std::map<int, ClientTiming>::const_iterator timing =
            clientTimings.find(fd);
        if (timing == clientTimings.end())
            return false;
        if (now - timing->second.lastActivity
            >= std::chrono::seconds(CLIENT_IDLE_TIMEOUT_SECONDS))
            return true;
        if (timing->second.requestActive
            && now - timing->second.requestStarted
                >= std::chrono::seconds(CLIENT_REQUEST_TIMEOUT_SECONDS))
            return true;
        return false;
    }

    void removeExpiredClients(
        const std::vector<Server>& hosting, std::vector<pollfd>& fds,
        std::map<int, std::string>& requests,
        std::map<int, ServerConfig>& configs, CgiHandler& cgi,
        std::map<int, CgiJob>& cgiJobs)
    {
        const std::chrono::steady_clock::time_point now =
            std::chrono::steady_clock::now();
        size_t i = 0;
        while (i < fds.size())
        {
            int fd = fds[i].fd;
            if (isListener(fd, hosting) || !clientHasExpired(fd, now))
            {
                ++i;
                continue;
            }
            removeClient(fd, fds, requests, configs, cgi, cgiJobs);
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
        clientTimings.clear();
        largePostClients.clear();
        largePostWaiters.clear();
        peerClosedClients.clear();
    }
}
