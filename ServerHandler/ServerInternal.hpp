#ifndef SERVER_INTERNAL_HPP
#define SERVER_INTERNAL_HPP

#include <set>
#include <cstddef>
#include <csignal>
#include <chrono>
#include <map>

namespace ServerInternal
{
    extern volatile sig_atomic_t shutdownRequested;
    const size_t LARGE_POST_THRESHOLD = 8 * 1024 * 1024;
    const size_t MAX_BUFFERED_LARGE_POSTS = 5;
    const int CLIENT_IDLE_TIMEOUT_SECONDS = 60;
    const int CLIENT_REQUEST_TIMEOUT_SECONDS = 60;

    struct ClientTiming
    {
        std::chrono::steady_clock::time_point lastActivity;
        std::chrono::steady_clock::time_point requestStarted;
        bool requestActive;

        ClientTiming();
    };

    extern std::map<int, ClientTiming> clientTimings;
    extern std::set<int> largePostClients;
    extern std::set<int> largePostWaiters;
    extern std::set<int> peerClosedClients;

    void installShutdownHandlers();
    void registerClient(int fd);
    void recordClientRead(int fd, bool startsRequest);
    void recordClientWrite(int fd);
    void completeClientRequest(int fd, bool anotherRequestBuffered);
    void forgetClient(int fd);
}

#endif
