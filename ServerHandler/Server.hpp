#ifndef SERVER_HPP
#define SERVER_HPP

#include "../httpParser/HttpParser.hpp"
#include "../ConfigParser/Configuration.hpp"
#include "../www/cgi/CgiHandler.hpp"
#include "../www/cgi/CgiResponse.hpp"
#include "Responder.hpp"
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
#include <cctype>
#include <deque>
#include <dirent.h>
#include <csignal>

struct Server
{
    int serverFD;
    ServerConfig conf;
};

void print_config(const std::vector<ServerConfig>& servers);
void Init(Server& server);
void runEventLoop(std::vector<Server>& hosting, std::vector<pollfd>& fds, std::map<int, std::string> &reqs);
void handleNewConnection(int serverFD, const ServerConfig& config, std::vector<pollfd>& fds, std::map<int, ServerConfig>& clientConfigs);
std::vector<pollfd> createPollFds(const std::vector<Server>& hosting);
void runHttpParser(int fd, std::map<int, std::string>& reqs,std::map<int, ServerConfig>& clientConfigs, CgiHandler& cgi,std::map<int, CgiJob>& cgiJobs);
bool handleGet(const HttpParser& http, const std::string& version, bool keepAlive, int clientFd, const ServerConfig& config);
bool handleDelete(int clientFd, const std::string& version, bool keepAlive, const std::string& rawTarget, const ServerConfig& config);
const LocationConfig* findMatchingLocation(const ServerConfig& config, const std::string& target, const std::string& method);
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
