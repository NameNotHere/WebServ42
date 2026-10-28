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
#include <vector>

struct CGIRequest
{
	std::string method;
	std::string script_path;
	std::string interpreter;
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

struct CgiJob
{
	pid_t pid;
	int stdinFd;
	int stdoutFd;
	std::string requestBody;
	size_t bodyOffset;
	std::string output;
	long long deadlineMs;
	long long killDeadlineMs;
	int waitStatus;
	int clientFd;
	std::string httpVersion;
	bool keepAlive;
	bool childReaped;
	bool outputEof;
	bool timedOut;
	bool ioError;
	bool terminationSent;
	bool childSignaled;
	std::string errorMessage;

	CgiJob();
};

class CgiHandler
{
private:
	std::string _interpreter;
	unsigned int _timeout_seconds;

	std::map<std::string, std::string> buildEnvironment(
		const CGIRequest& request) const;

public:
	CgiHandler();
	CgiHandler(const std::string& interpreter,
			   unsigned int timeout_seconds = 5);

	bool isCGI(const std::string& path) const;
	void setInterpreter(const std::string& interpreter);
	void setTimeout(unsigned int timeout_seconds);

	bool start(const CGIRequest& request, CgiJob& job) const;
	void writeInput(CgiJob& job) const;
	void readOutput(CgiJob& job) const;
	void pipeError(CgiJob& job, bool inputPipe) const;
	void update(CgiJob& job) const;
	bool complete(const CgiJob& job) const;
	CGIResult result(const CgiJob& job) const;
	void cancel(CgiJob& job) const;
};

#endif
