#ifndef SERVER_HPP
#define SERVER_HPP

#include "../Response/Response.hpp"
#include "../httpParser/HttpParser.hpp"
#include "../ConfigParser/Configuration.hpp"
#include <unistd.h>
#include <iostream>
#include <poll.h>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <algorithm>
#include <utility>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <cctype>

struct Server
{
    int serverFD;
    ServerConfig conf;
};

bool buildResponse(const HttpParser& http, int clientFd, bool& closeConnection, const std::string& root = ".");
void print_config(const std::vector<ServerConfig>& servers);
void Init(Server& server);
void runEventLoop(std::vector<Server>& hosting, std::vector<pollfd>& fds, std::map<int, std::string> &reqs);
void handleNewConnection (int serverFD, const ServerConfig& config, std::vector<pollfd>& fds, std::map<int, std::string>& clientRoots);
std::vector<pollfd> createPollFds(const std::vector<Server>& hosting);
void runHttpParser(int fd, size_t& i, std::map<int, std::string> &reqs, std::vector<pollfd>& fds, const std::map<int, std::string>& clientRoots);
std::string makeErrorResponse(int code, const std::string& version, bool keepAlive, const std::vector<std::pair<std::string, std::string>>& extraHeaders = {});
bool sendErrorResponse(int clientFd, const std::string& version, bool keepAlive, int code);
std::string statusText(int code);
bool handleGet(const HttpParser& http, const std::string& version, bool keepAlive, int clientFd, const std::string& root);
bool handleDelete(int clientFd, const std::string& version, bool keepAlive, const std::string& rawTarget, const std::string& root);
ssize_t sendAll(int fd, const char* buf, size_t len);
std::string normalizeTarget(const std::string& rawTarget);
std::string toLower(std::string s);
std::string resolveRequestedPath(const std::string& root, const std::string& target);
bool sendFileBody(int clientFd, int fd_in, off_t fileSize);

#endif
