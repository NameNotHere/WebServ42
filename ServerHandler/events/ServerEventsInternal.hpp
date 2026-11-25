#ifndef SERVER_EVENTS_INTERNAL_HPP
#define SERVER_EVENTS_INTERNAL_HPP

#include "../core/Server.hpp"
#include "../core/ServerInternal.hpp"

namespace ServerEventsInternal
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

    void promoteLargePostWaiters(
        const std::vector<pollfd>& fds,
        const std::map<int, std::string>& requests);
    void removeClient(int fd, std::vector<pollfd>& fds,
                      std::map<int, std::string>& requests,
                      std::map<int, ServerConfig>& configs, CgiHandler& cgi,
                      std::map<int, CgiJob>& cgiJobs);
    bool isListener(int fd, const std::vector<Server>& hosting);
    void removeExpiredClients(
        const std::vector<Server>& hosting, std::vector<pollfd>& fds,
        std::map<int, std::string>& requests,
        std::map<int, ServerConfig>& configs, CgiHandler& cgi,
        std::map<int, CgiJob>& cgiJobs);
    void cleanupEventLoop(
        const std::vector<Server>& hosting, std::vector<pollfd>& fds,
        std::map<int, std::string>& requests,
        std::map<int, ServerConfig>& configs, CgiHandler& cgi,
        std::map<int, CgiJob>& cgiJobs);

    void updateClientPollEvents(
        const std::vector<Server>& hosting, std::vector<pollfd>& fds,
        std::map<int, std::string>& requests,
        std::map<int, ServerConfig>& clientConfigs, CgiHandler& cgi,
        std::map<int, CgiJob>& cgiJobs);
    void appendClientPollSources(
        const std::vector<Server>& hosting, const std::vector<pollfd>& fds,
        std::vector<pollfd>& pollFds, std::vector<PollSource>& sources);
    void appendCgiPollSources(
        const std::map<int, CgiJob>& cgiJobs,
        std::vector<pollfd>& pollFds, std::vector<PollSource>& sources);
    void handlePollEvent(
        const PollSource& source, short events,
        std::vector<Server>& hosting, std::vector<pollfd>& fds,
        std::map<int, std::string>& requests,
        std::map<int, ServerConfig>& clientConfigs, CgiHandler& cgi,
        std::map<int, CgiJob>& cgiJobs);
    void completeCgiJobs(
        CgiHandler& cgi, std::map<int, CgiJob>& cgiJobs,
        std::map<int, std::string>& requests,
        std::map<int, ServerConfig>& clientConfigs,
        std::vector<pollfd>& fds);
}

#endif
