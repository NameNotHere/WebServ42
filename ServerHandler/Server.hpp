#ifndef SERVER_HPP
#define SERVER_HPP

#include "../Response/Response.hpp"
#include "../httpParser/HttpParser.hpp"
#include "../ConfigParser/Configuration.hpp"
#include "../www/cgi/CgiHandler.hpp"
#include "../www/cgi/CgiResponse.hpp"
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

bool buildResponse(const HttpParser& http, int clientFd, bool& closeConnection, const ServerConfig& config);
void print_config(const std::vector<ServerConfig>& servers);
void Init(Server& server);
void runEventLoop(std::vector<Server>& hosting, std::vector<pollfd>& fds, std::map<int, std::string> &reqs);
void handleNewConnection(int serverFD, const ServerConfig& config, std::vector<pollfd>& fds, std::map<int, ServerConfig>& clientConfigs);
std::vector<pollfd> createPollFds(const std::vector<Server>& hosting);
void runHttpParser(int fd, size_t& i, std::map<int, std::string>& reqs, std::vector<pollfd>& fds, std::map<int, ServerConfig>& clientConfigs);
std::string makeErrorResponse(int code, const std::string& version, bool keepAlive, const std::vector<std::pair<std::string, std::string>>& extraHeaders = {});
bool sendErrorResponse(int clientFd, const std::string& version, bool keepAlive, int code);
std::string statusText(int code);
bool handleGet(const HttpParser& http, const std::string& version, bool keepAlive, int clientFd, const ServerConfig& config);
bool handleDelete(int clientFd, const std::string& version, bool keepAlive, const std::string& rawTarget, const ServerConfig& config);
ssize_t sendAll(int fd, const char* buf, size_t len);
std::string normalizeTarget(const std::string& rawTarget);
std::string toLower(std::string s);
std::string resolveRequestedPath(const std::string& root, const std::string& target);
bool sendFileBody(int clientFd, int fd_in, off_t fileSize);
const LocationConfig* findMatchingLocation(const ServerConfig& config, const std::string& target);
bool isPostAllowed(const ServerConfig& config, const LocationConfig& location);
bool getUploadFilename(const std::string& target, const LocationConfig& location, std::string& filename);
int openUploadDirectory(const LocationConfig& location);
int openUploadFile(int dirFd, const std::string& filename, bool& existed);
bool writeRequestBody(int fileFd, const std::string& body);
int saveUploadedFile(const LocationConfig& location, const std::string& filename, const std::string& body);
bool sendPostResponse(int clientFd, const std::string& version, bool keepAlive, int status);
bool handlePost(const HttpParser& http, const std::string& version, bool keepAlive, int clientFd, const ServerConfig& config);
bool isMethodAllowed(const ServerConfig& config, const LocationConfig* location, const std::string& method);
bool decodeUriPath(const std::string& encoded, std::string& decoded);

#endif
