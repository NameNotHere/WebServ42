#include "CgiHandler.hpp"

namespace
{
    const size_t MAX_CGI_OUTPUT = 16 * 1024 * 1024;
    const int KILL_GRACE_MS = 1000;

    //return monotonic milliseconds so wall-clock changes cannot extend a CGI run (not sure what
    //this exactly means still)
    long long nowMilliseconds()
    {
        struct timespec value;
        if (clock_gettime(CLOCK_MONOTONIC, &value) == -1)
            return 0;
        return static_cast<long long>(value.tv_sec) * 1000
            + value.tv_nsec / 1000000;
    }

    //set nonblocking mode
    bool setNonBlocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1)
            return false;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
    }

    //return the directory used as the CGI process working directory
    std::string scriptDirectory(const std::string& script)
    {
        size_t slash = script.find_last_of('/');
        if (slash == std::string::npos)
            return ".";
        if (slash == 0)
            return "/";
        return script.substr(0, slash);
    }

    //return the argument that remains valid after the child changes directory
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

    //signal the CGI process and any descendants
    void signalProcessGroup(pid_t pid, int signalNumber)
    {
        if (kill(-pid, signalNumber) == -1)
            kill(pid, signalNumber);
    }

    //find a request header
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
                {
                    if (std::tolower(static_cast<unsigned char>(it->first[i]))
                        != std::tolower(static_cast<unsigned char>(wanted[i])))
                    {
                        same = false;
                        break;
                    }
                }
                if (same)
                    return it->second;
            }
            return "";
    }
}

CgiHandler::CgiHandler()
    : _interpreter(""), _timeout_seconds(5)
{
}

CgiHandler::CgiHandler(const std::string& interpreter, unsigned int timeout_seconds)
    : _interpreter(interpreter), _timeout_seconds(timeout_seconds)
{
}

CgiHandler::CgiHandler(const CgiHandler& other)
    : _interpreter(other._interpreter), _timeout_seconds(other._timeout_seconds)
{
}

CgiHandler& CgiHandler::operator=(const CgiHandler& other)
{
    if (this != &other)
    {
        _interpreter     = other._interpreter;
        _timeout_seconds = other._timeout_seconds;
    }
    return *this;
}

CgiHandler::~CgiHandler()
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
    //detect the standalone extension, may need to optimize because not looking for all of them
    std::string cleanPath = path.substr(0, path.find('?'));
    size_t dot = cleanPath.find_last_of('.');
    if (dot == std::string::npos)
        return false;

    std::string ext = cleanPath.substr(dot);
    for (size_t i = 0; i < ext.size(); ++i)
        ext[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[i])));

    if (ext == ".cgi" || ext == ".py" || ext == ".php" || ext == ".pl")
        return true;

    return false;
}

std::map<std::string, std::string>
CgiHandler::buildEnvironment(const CGIRequest& request) const
{
    std::map<std::string, std::string> env;


    env["GATEWAY_INTERFACE"] = "CGI/1.1";
    env["SERVER_PROTOCOL"]   = request.server_protocol;
    env["SERVER_NAME"]       = request.server_name;
    env["SERVER_PORT"]       = request.server_port;
    env["SERVER_SOFTWARE"]   = request.server_software;
    env["REQUEST_METHOD"]    = request.method;
    env["SCRIPT_FILENAME"]   = request.script_path;
    env["SCRIPT_NAME"]       = request.script_name.empty()
        ? request.script_path : request.script_name;
    env["PATH_INFO"]         = request.path_info;
    env["PATH_TRANSLATED"]   = request.path_translated;
    env["QUERY_STRING"]      = request.query_string;
    env["REMOTE_ADDR"]       = request.remote_addr;
    env["REMOTE_HOST"]       = request.remote_host;
    env["REMOTE_USER"]       = request.remote_user;
    env["AUTH_TYPE"]         = request.auth_type;

    if (!request.body.empty())
        env["CONTENT_LENGTH"] = std::to_string(request.body.size());
    env["REDIRECT_STATUS"]   = "200";

    // Convert ordinary HTTP headers into CGI
    for (std::map<std::string, std::string>::const_iterator it = request.headers.begin();
         it != request.headers.end(); ++it)
    {
        if (it->first == "content-type" || it->first == "content-length")
            continue;
        std::string key = "HTTP_" + it->first;
        for (size_t i = 0; i < key.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(key[i]);
            if (c == '-')
                key[i] = '_';
            else
                key[i] = static_cast<char>(std::toupper(c));
        }

        env[key] = it->second;
    }
    std::string contentType = headerValue(request.headers, "content-type");
    if (!contentType.empty())
        env["CONTENT_TYPE"] = contentType;
    return env;
}

CGIResult CgiHandler::runProcess(
    const CGIRequest& request,
    const std::map<std::string, std::string>& environment) const
{
    CGIResult result;
    result.exit_status = -1;
    result.timed_out = false;
    result.io_error = false;
    result.child_signaled = false;

    int inPipe[2];
    int outPipe[2];

    if (pipe(inPipe) == -1)
        return result;

    if (pipe(outPipe) == -1)
    {
        close(inPipe[0]); close(inPipe[1]);
        return result;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        // fork failed — clean up fds.
        close(inPipe[0]);  close(inPipe[1]);
        close(outPipe[0]); close(outPipe[1]);
        return result;
    }

    if (pid == 0)
    {
        setpgid(0, 0);
        if (dup2(inPipe[0], STDIN_FILENO) == -1)
            _exit(1);
        if (dup2(outPipe[1], STDOUT_FILENO) == -1)
            _exit(1);
        close(inPipe[0]);  close(inPipe[1]);
        close(outPipe[0]); close(outPipe[1]);
        if (chdir(scriptDirectory(request.script_path).c_str()) == -1)
            _exit(126);

        char* argv[3];
        argv[0] = const_cast<char*>(_interpreter.c_str());
        std::string argument = scriptArgument(request.script_path);
        argv[1] = const_cast<char*>(argument.c_str());
        argv[2] = NULL;
        std::vector<std::string> envStrings;
        for (std::map<std::string, std::string>::const_iterator it = environment.begin();
             it != environment.end(); ++it)
        {
            envStrings.push_back(it->first + "=" + it->second);
        }

        std::vector<char*> envp;
        for (size_t i = 0; i < envStrings.size(); ++i)
            envp.push_back(const_cast<char*>(envStrings[i].c_str()));
        envp.push_back(NULL);
        execve(_interpreter.c_str(), argv, envp.data());
        _exit(127);
    }
    close(inPipe[0]);
    close(outPipe[1]);
    setpgid(pid, pid);
    int inputFd = inPipe[1];
    int outputFd = outPipe[0];
    if (!setNonBlocking(inputFd) || !setNonBlocking(outputFd))
    {
        result.io_error = true;
        result.error_message = "failed to set CGI pipes nonblocking";
        signalProcessGroup(pid, SIGKILL);
        closeDescriptor(inputFd);
        closeDescriptor(outputFd);
        while (waitpid(pid, NULL, 0) == -1 && errno == EINTR) {}
        return result;
    }

    //move stdin and stdout concurrently so neither pipe can deadlock the other
    std::string output;
    size_t bodyOffset = 0;
    char buffer[4096];
    bool childReaped = false;
    bool outputClosed = false;
    bool terminationSent = false;
    int status = 0;
    if (request.body.empty())
        closeDescriptor(inputFd);
    const long long start = nowMilliseconds();
    long long terminationTime = 0;

    while (!outputClosed || !childReaped)
    {
        long long now = nowMilliseconds();
        long long elapsed = now - start;
        if (!terminationSent && elapsed >= static_cast<long long>(_timeout_seconds) * 1000)
        {
            result.timed_out = true;
            terminationSent = true;
            terminationTime = now;
            signalProcessGroup(pid, SIGTERM);
        }
        else if (terminationSent && now - terminationTime >= KILL_GRACE_MS && !childReaped)
            signalProcessGroup(pid, SIGKILL);

        struct pollfd fds[2];
        nfds_t count = 0;
        int inputIndex = -1;
        int outputIndex = -1;
        if (inputFd != -1)
        {
            inputIndex = static_cast<int>(count);
            fds[count].fd = inputFd;
            fds[count].events = POLLOUT;
            fds[count].revents = 0;
            ++count;
        }
        if (outputFd != -1)
        {
            outputIndex = static_cast<int>(count);
            fds[count].fd = outputFd;
            fds[count].events = POLLIN;
            fds[count].revents = 0;
            ++count;
        }

        int pollTimeout = 100;
        if (!terminationSent)
        {
            long long remaining = static_cast<long long>(_timeout_seconds) * 1000 - elapsed;
            if (remaining < pollTimeout)
                pollTimeout = remaining > 0 ? static_cast<int>(remaining) : 0;
        }
        int pollResult = count == 0 ? 0 : poll(fds, count, pollTimeout);
        if (pollResult == -1)
        {
            if (errno == EINTR)
                continue;
            result.io_error = true;
            result.error_message = "poll failed for CGI pipes";
            break;
        }

        if (outputIndex != -1 && (fds[outputIndex].revents & (POLLIN | POLLHUP | POLLERR)))
        {
            while (true)
            {
                ssize_t bytesRead = read(outputFd, buffer, sizeof(buffer));
                if (bytesRead > 0)
                {
                    if (output.size() + static_cast<size_t>(bytesRead) > MAX_CGI_OUTPUT)
                    {
                        result.io_error = true;
                        result.error_message = "CGI output exceeded the configured limit";
                        signalProcessGroup(pid, SIGKILL);
                        terminationSent = true;
                        outputClosed = true;
                        closeDescriptor(outputFd);
                        break;
                    }
                    output.append(buffer, static_cast<size_t>(bytesRead));
                    continue;
                }
                if (bytesRead == 0)
                {
                    outputClosed = true;
                    closeDescriptor(outputFd);
                }
                else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    result.io_error = true;
                    result.error_message = "failed to read CGI output";
                    outputClosed = true;
                    closeDescriptor(outputFd);
                }
                break;
            }
        }

        if (inputIndex != -1 && (fds[inputIndex].revents & (POLLOUT | POLLERR | POLLHUP)))
        {
            ssize_t bytesWritten = write(inputFd, request.body.data() + bodyOffset,
                request.body.size() - bodyOffset);
            if (bytesWritten > 0)
                bodyOffset += static_cast<size_t>(bytesWritten);
            else if (bytesWritten == -1 && (errno == EPIPE || errno == ECONNRESET))
            {
                result.io_error = true;
                result.error_message = "CGI closed stdin before the request body was sent";
                closeDescriptor(inputFd);
            }
            else if (bytesWritten == -1 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                result.io_error = true;
                result.error_message = "failed to write CGI input";
                closeDescriptor(inputFd);
            }
            if (bodyOffset == request.body.size())
                closeDescriptor(inputFd);
        }

        if (!childReaped)
        {
            pid_t waited = waitpid(pid, &status, WNOHANG);
            if (waited == pid)
                childReaped = true;
            else if (waited == -1 && errno != EINTR)
            {
                result.io_error = true;
                result.error_message = "waitpid failed for CGI process";
                childReaped = true;
            }
        }
        if (childReaped && inputFd != -1)
            closeDescriptor(inputFd);
    }

    closeDescriptor(inputFd);
    closeDescriptor(outputFd);
    while (!childReaped)
    {
        pid_t waited = waitpid(pid, &status, 0);
        if (waited == pid)
            childReaped = true;
        else if (waited == -1 && errno != EINTR)
        {
            result.io_error = true;
            result.error_message = "waitpid failed while reaping CGI process";
            break;
        }
    }
    if (childReaped && WIFEXITED(status))
        result.exit_status = WEXITSTATUS(status);
    else if (childReaped && WIFSIGNALED(status))
        result.child_signaled = true;
    result.raw_output = output;
    return result;
}

CGIResult CgiHandler::execute(const CGIRequest& request) const
{
    // A closed CGI stdin must be reported as EPIPE
    signal(SIGPIPE, SIG_IGN);
    std::map<std::string, std::string> env = buildEnvironment(request);
    return runProcess(request, env);
}