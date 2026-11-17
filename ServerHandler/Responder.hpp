#ifndef RESPONDER_HPP
#define RESPONDER_HPP

#include "../ConfigParser/Configuration.hpp"
#include "../httpParser/HttpParser.hpp"
#include "../www/cgi/CgiHandler.hpp"
#include "../www/cgi/CgiResponse.hpp"
#include <map>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

bool buildResponse(HttpParser& http, int clientFd, bool& keepAlive, const ServerConfig& config, CgiHandler& cgi, std::map<int, CgiJob>& cgiJobs);
std::string makeErrorResponse(int code, const std::string& version, bool keepAlive, const std::vector<std::pair<std::string, std::string> >& extraHeaders = std::vector<std::pair<std::string, std::string> >(), const ServerConfig* config = NULL);
bool sendErrorResponse(int clientFd, const std::string& version, bool keepAlive, int code,const ServerConfig* config = NULL, const std::vector<std::pair<std::string, std::string> >& extraHeaders = std::vector<std::pair<std::string, std::string> >());
bool queueCgiResponse(int clientFd, const CgiJob& job, CGIResult& result, const ServerConfig* config = NULL);

bool hasPendingOutput(int clientFd);
bool flushPendingOutput(int clientFd);
void clearPendingOutput(int clientFd);
void closeAfterPendingOutput(int clientFd);
bool shouldCloseAfterOutput(int clientFd);
bool sendFileBody(int clientFd, int fileFd, off_t fileSize);
ssize_t sendAll(int fd, const char* buffer, size_t length);

std::string statusText(int code);
std::string allowHeaderValue(const ServerConfig& config, const LocationConfig* location);
std::string findHeaderValue(const std::map<std::string, std::string>& headers, const std::string& name);
std::string normalizeTarget(const std::string& rawTarget);
std::string resolveRequestedPath(const std::string& root, const std::string& target);
std::string toLower(std::string value);
std::string toUpper(std::string value);

#endif
