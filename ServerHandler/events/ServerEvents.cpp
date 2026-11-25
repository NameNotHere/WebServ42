#include "../core/Server.hpp"
#include "../core/ServerInternal.hpp"
#include "ServerEventsInternal.hpp"
#include <cerrno>

using namespace ServerInternal;
using namespace ServerEventsInternal;

bool runEventLoop(std::vector<Server>& hosting, std::vector<pollfd>& fds,
                  std::map<int, std::string>& reqs)
{
    std::map<int, ServerConfig> clientConfigs;
    std::map<int, CgiJob> cgiJobs;
    CgiHandler cgi("/usr/bin/python3", 5);
    bool pollFailed = false;

    while (!shutdownRequested)
    {
        removeExpiredClients(hosting, fds, reqs, clientConfigs, cgi, cgiJobs);
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
            if (errno == EINTR)
                continue;
            int pollError = errno;
            std::cerr << "poll() failed: " << std::strerror(pollError) << '\n';
            pollFailed = true;
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
    return !pollFailed;
}
