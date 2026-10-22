#include "CgiHandler.hpp"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace
{
	const size_t MAX_CGI_OUTPUT = 16 * 1024 * 1024;
	const long long KILL_GRACE_MS = 1000;

	long long nowMilliseconds()
	{
		struct timespec value;
		if (clock_gettime(CLOCK_MONOTONIC, &value) == -1)
			return 0;
		return static_cast<long long>(value.tv_sec) * 1000
			+ value.tv_nsec / 1000000;
	}

	bool setNonBlocking(int fd)
	{
		int flags = fcntl(fd, F_GETFL, 0);
		return flags != -1 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
	}

	std::string scriptDirectory(const std::string& script)
	{
		size_t slash = script.find_last_of('/');
		if (slash == std::string::npos)
			return ".";
		if (slash == 0)
			return "/";
		return script.substr(0, slash);
	}

	std::string scriptArgument(const std::string& script)
	{
		if (!script.empty() && script[0] == '/')
			return script;
		size_t slash = script.find_last_of('/');
		if (slash == std::string::npos)
			return script;
		return script.substr(slash + 1);
	}

	void closeDescriptor(int& fd)
	{
		if (fd != -1)
		{
			close(fd);
			fd = -1;
		}
	}

	void signalProcessGroup(pid_t pid, int signalNumber)
	{
		if (pid <= 0)
			return;
		if (kill(-pid, signalNumber) == -1)
			kill(pid, signalNumber);
	}

	std::string headerValue(const std::map<std::string, std::string>& headers,
		const std::string& wanted)
	{
		for (std::map<std::string, std::string>::const_iterator it = headers.begin();
			it != headers.end(); ++it)
		{
			if (it->first.size() != wanted.size())
				continue;
			bool same = true;
			for (size_t i = 0; i < wanted.size(); ++i)
				if (std::tolower(static_cast<unsigned char>(it->first[i]))
					!= std::tolower(static_cast<unsigned char>(wanted[i])))
					same = false;
			if (same)
				return it->second;
		}
		return "";
	}
	bool createCgiPipes(int inputPipe[2], int outputPipe[2])
	{
		inputPipe[0] = inputPipe[1] = -1;
		outputPipe[0] = outputPipe[1] = -1;
		if (pipe(inputPipe) == -1)
			return false;
		if (pipe(outputPipe) == -1)
		{
			closeDescriptor(inputPipe[0]);
			closeDescriptor(inputPipe[1]);
			return false;
		}
		return true;
	}

	void closeCgiPipes(int inputPipe[2], int outputPipe[2])
	{
		closeDescriptor(inputPipe[0]);
		closeDescriptor(inputPipe[1]);
		closeDescriptor(outputPipe[0]);
		closeDescriptor(outputPipe[1]);
	}

	void buildEnvironmentPointers(
		const std::map<std::string, std::string>& environment,
		std::vector<std::string>& envStrings, std::vector<char*>& envp)
	{
		for (std::map<std::string, std::string>::const_iterator it = environment.begin();
			it != environment.end(); ++it)
			envStrings.push_back(it->first + "=" + it->second);
		for (size_t i = 0; i < envStrings.size(); ++i)
			envp.push_back(const_cast<char*>(envStrings[i].c_str()));
		envp.push_back(NULL);
	}

	void executeCgiChild(const std::string& interpreter,
		const std::string& scriptPath, const std::string& argument,
		std::vector<char*>& envp, int inputPipe[2], int outputPipe[2])
	{
		setpgid(0, 0);
		if (dup2(inputPipe[0], STDIN_FILENO) == -1
			|| dup2(outputPipe[1], STDOUT_FILENO) == -1)
			_exit(1);
		close(inputPipe[0]);
		close(inputPipe[1]);
		close(outputPipe[0]);
		close(outputPipe[1]);
		if (chdir(scriptDirectory(scriptPath).c_str()) == -1)
			_exit(126);

		char* argv[3];
		argv[0] = const_cast<char*>(interpreter.c_str());
		argv[1] = const_cast<char*>(argument.c_str());
		argv[2] = NULL;
		execve(interpreter.c_str(), argv, envp.data());
		_exit(127);
	}

	bool configureParentPipes(pid_t pid, int inputPipe[2], int outputPipe[2])
	{
		closeDescriptor(inputPipe[0]);
		closeDescriptor(outputPipe[1]);
		setpgid(pid, pid);
		if (!setNonBlocking(inputPipe[1]) || !setNonBlocking(outputPipe[0]))
		{
			signalProcessGroup(pid, SIGKILL);
			closeDescriptor(inputPipe[1]);
			closeDescriptor(outputPipe[0]);
			while (waitpid(pid, NULL, 0) == -1 && errno == EINTR) {}
			return false;
		}
		return true;
	}

	void initializeCgiJob(CgiJob& job, const CGIRequest& request, pid_t pid,
		int inputPipe[2], int outputPipe[2], unsigned int timeoutSeconds)
	{
		job = CgiJob();
		job.pid = pid;
		job.stdinFd = inputPipe[1];
		job.stdoutFd = outputPipe[0];
		job.requestBody = request.body;
		job.deadlineMs = nowMilliseconds()
			+ static_cast<long long>(timeoutSeconds) * 1000;
		if (job.requestBody.empty())
			closeDescriptor(job.stdinFd);
	}

}

CgiJob::CgiJob()
	: pid(-1), stdinFd(-1), stdoutFd(-1), bodyOffset(0), deadlineMs(0),
	  killDeadlineMs(0), waitStatus(0), clientFd(-1), keepAlive(false),
	  childReaped(false), outputEof(false), timedOut(false), ioError(false),
	  terminationSent(false), childSignaled(false)
{
}

CgiHandler::CgiHandler()
	: _interpreter(""), _timeout_seconds(5)
{
}

CgiHandler::CgiHandler(const std::string& interpreter,
	unsigned int timeout_seconds)
	: _interpreter(interpreter), _timeout_seconds(timeout_seconds)
{
}

void CgiHandler::setInterpreter(const std::string& interpreter)
{
	_interpreter = interpreter;
}

void CgiHandler::setTimeout(unsigned int timeout_seconds)
{
	_timeout_seconds = timeout_seconds;
}

bool CgiHandler::isCGI(const std::string& path) const
{
	std::string cleanPath = path.substr(0, path.find('?'));
	size_t dot = cleanPath.find_last_of('.');
	if (dot == std::string::npos)
		return false;
	std::string ext = cleanPath.substr(dot);
	for (size_t i = 0; i < ext.size(); ++i)
		ext[i] = static_cast<char>(std::tolower(
			static_cast<unsigned char>(ext[i])));
	return ext == ".cgi" || ext == ".py" || ext == ".php" || ext == ".pl";
}

std::map<std::string, std::string>
CgiHandler::buildEnvironment(const CGIRequest& request) const
{
	std::map<std::string, std::string> env;
	env["GATEWAY_INTERFACE"] = "CGI/1.1";
	env["SERVER_PROTOCOL"] = request.server_protocol;
	env["SERVER_NAME"] = request.server_name;
	env["SERVER_PORT"] = request.server_port;
	env["SERVER_SOFTWARE"] = request.server_software;
	env["REQUEST_METHOD"] = request.method;
	env["SCRIPT_FILENAME"] = request.script_path;
	env["SCRIPT_NAME"] = request.script_name.empty()
		? request.script_path : request.script_name;
	env["PATH_INFO"] = request.path_info;
	env["PATH_TRANSLATED"] = request.path_translated;
	env["QUERY_STRING"] = request.query_string;
	env["REMOTE_ADDR"] = request.remote_addr;
	env["REMOTE_HOST"] = request.remote_host;
	env["REMOTE_USER"] = request.remote_user;
	env["AUTH_TYPE"] = request.auth_type;
	if (!request.body.empty())
		env["CONTENT_LENGTH"] = std::to_string(request.body.size());
	env["REDIRECT_STATUS"] = "200";

	for (std::map<std::string, std::string>::const_iterator it = request.headers.begin();
		it != request.headers.end(); ++it)
	{
		if (it->first == "content-type" || it->first == "content-length")
			continue;
		std::string key = "HTTP_" + it->first;
		for (size_t i = 0; i < key.size(); ++i)
		{
			unsigned char c = static_cast<unsigned char>(key[i]);
			key[i] = c == '-' ? '_' : static_cast<char>(std::toupper(c));
		}
		env[key] = it->second;
	}
	std::string contentType = headerValue(request.headers, "content-type");
	if (!contentType.empty())
		env["CONTENT_TYPE"] = contentType;
	return env;
}

bool CgiHandler::start(const CGIRequest& request, CgiJob& job) const
{
	int inputPipe[2] = { -1, -1 };
	int outputPipe[2] = { -1, -1 };
	if (!createCgiPipes(inputPipe, outputPipe))
		return false;

	std::map<std::string, std::string> environment = buildEnvironment(request);
	std::vector<std::string> envStrings;
	std::vector<char*> envp;
	buildEnvironmentPointers(environment, envStrings, envp);
	std::string argument = scriptArgument(request.script_path);

	pid_t pid = fork();
	if (pid == -1)
	{
		closeCgiPipes(inputPipe, outputPipe);
		return false;
	}
	if (pid == 0)
		executeCgiChild(_interpreter, request.script_path, argument, envp,
			inputPipe, outputPipe);

	if (!configureParentPipes(pid, inputPipe, outputPipe))
		return false;
	initializeCgiJob(job, request, pid, inputPipe, outputPipe,
		_timeout_seconds);
	return true;
}

void CgiHandler::writeInput(CgiJob& job) const
{
	if (job.stdinFd == -1)
		return;
	if (job.bodyOffset >= job.requestBody.size())
	{
		closeDescriptor(job.stdinFd);
		return;
	}
	ssize_t written = write(job.stdinFd,
		job.requestBody.data() + job.bodyOffset,
		job.requestBody.size() - job.bodyOffset);
	if (written > 0)
	{
		job.bodyOffset += static_cast<size_t>(written);
		if (job.bodyOffset == job.requestBody.size())
			closeDescriptor(job.stdinFd);
	}
}

void CgiHandler::readOutput(CgiJob& job) const
{
	if (job.stdoutFd == -1)
		return;
	char buffer[4096];
	ssize_t bytesRead = read(job.stdoutFd, buffer, sizeof(buffer));
	if (bytesRead > 0)
	{
		if (job.output.size() + static_cast<size_t>(bytesRead) > MAX_CGI_OUTPUT)
		{
			job.ioError = true;
			job.errorMessage = "CGI output exceeded the configured limit";
			signalProcessGroup(job.pid, SIGKILL);
			closeDescriptor(job.stdoutFd);
			job.outputEof = true;
			return;
		}
		job.output.append(buffer, static_cast<size_t>(bytesRead));
	}
	else if (bytesRead == 0)
	{
		closeDescriptor(job.stdoutFd);
		job.outputEof = true;
	}
}

void CgiHandler::pipeError(CgiJob& job, bool inputPipe) const
{
	job.ioError = true;
	job.errorMessage = inputPipe
		? "CGI stdin pipe reported an error"
		: "CGI stdout pipe reported an error";
	if (inputPipe)
		closeDescriptor(job.stdinFd);
	else
	{
		closeDescriptor(job.stdoutFd);
		job.outputEof = true;
	}
}

void CgiHandler::update(CgiJob& job) const
{
	long long now = nowMilliseconds();
	if (!job.terminationSent && now >= job.deadlineMs)
	{
		job.timedOut = true;
		job.terminationSent = true;
		job.killDeadlineMs = now + KILL_GRACE_MS;
		signalProcessGroup(job.pid, SIGTERM);
	}
	else if (job.terminationSent && !job.childReaped
		&& now >= job.killDeadlineMs)
		signalProcessGroup(job.pid, SIGKILL);

	if (!job.childReaped)
	{
		pid_t waited = waitpid(job.pid, &job.waitStatus, WNOHANG);
		if (waited == job.pid)
		{
			job.childReaped = true;
			closeDescriptor(job.stdinFd);
		}
		else if (waited == -1 && errno != EINTR)
		{
			job.ioError = true;
			job.errorMessage = "waitpid failed for CGI process";
			job.childReaped = true;
			closeDescriptor(job.stdinFd);
		}
	}
}

bool CgiHandler::complete(const CgiJob& job) const
{
	return job.childReaped && job.outputEof;
}

CGIResult CgiHandler::result(const CgiJob& job) const
{
	CGIResult result;
	result.exit_status = -1;
	result.timed_out = job.timedOut;
	result.io_error = job.ioError;
	result.child_signaled = job.childSignaled;
	result.error_message = job.errorMessage;
	result.raw_output = job.output;
	if (job.childReaped && WIFEXITED(job.waitStatus))
		result.exit_status = WEXITSTATUS(job.waitStatus);
	else if (job.childReaped && WIFSIGNALED(job.waitStatus))
		result.child_signaled = true;
	return result;
}

void CgiHandler::cancel(CgiJob& job) const
{
	if (job.pid > 0 && !job.childReaped)
	{
		signalProcessGroup(job.pid, SIGKILL);
		while (waitpid(job.pid, NULL, 0) == -1 && errno == EINTR) {}
	}
	closeDescriptor(job.stdinFd);
	closeDescriptor(job.stdoutFd);
	job.childReaped = true;
	job.outputEof = true;
}
