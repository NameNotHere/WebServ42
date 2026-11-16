#ifndef SERVER_INTERNAL_HPP
#define SERVER_INTERNAL_HPP

#include <set>
#include <cstddef>

namespace ServerInternal
{
    const size_t LARGE_POST_THRESHOLD = 8 * 1024 * 1024;
    const size_t MAX_BUFFERED_LARGE_POSTS = 5;
    extern std::set<int> largePostClients;
    extern std::set<int> largePostWaiters;
    extern std::set<int> peerClosedClients;
}

#endif
