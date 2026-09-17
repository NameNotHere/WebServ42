#ifndef CGI_HANDLER_HPP
#define CGI_HANDLER_HPP

#include <map>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <cerrno>
#include <cctype>
#include <vector>
#include <string>
#include <algorithm>

struct CGIRequest
{
	std::string method;
	std::string script_path;
	std::string script_name;
	std::string path_info;
	std::string query_string;
	std::string body;
	std::string server_name;
	std::string server_port;
	std::string server_protocol;
	std::string server_software;
	std::string remote_addr;
	std::string remote_host;
	std::string remote_user;
	std::string auth_type;
	std::string path_translated;
	std::map<std::string, std::string> headers;
};

struct CGIResult
{
	int exit_status;
	bool timed_out;
	bool io_error;
	bool child_signaled;
	std::string error_message;
	std::string raw_output;
};

class CgiHandler
{
private:
	std::string _interpreter;
	unsigned int _timeout_seconds;

	std::map<std::string, std::string> buildEnvironment(
		const CGIRequest& request) const;
	CGIResult runProcess(
		const CGIRequest& request,
		const std::map<std::string, std::string>& environment) const;

public:
	CgiHandler();
	CgiHandler(const std::string& interpreter,
			   unsigned int timeout_seconds = 5);
	CgiHandler(const CgiHandler& other);
	CgiHandler& operator=(const CgiHandler& other);
	~CgiHandler();

	void setInterpreter(const std::string& interpreter);
	void setTimeout(unsigned int timeout_seconds);

	bool isCGI(const std::string& path) const;
	CGIResult execute(const CGIRequest& request) const;
};

#endif
