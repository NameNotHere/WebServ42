#include "Server.hpp"

const LocationConfig* findMatchingLocation(const ServerConfig& config, const std::string& target)
{
    size_t queryPos = target.find('?');
    std::string path = target.substr(0, queryPos);

    const LocationConfig* best = NULL;

    for (size_t i = 0; i < config.locations.size(); ++i)
    {
        const LocationConfig& location = config.locations[i];
        const std::string& prefix = location.path;

        if (prefix.empty())
            continue;

        if (path.compare(0, prefix.size(), prefix) != 0)
            continue;

        // Avoid matching /upload when the path is /upload2.
        if (prefix[prefix.size() - 1] != '/' &&
            path.size() > prefix.size() &&
            path[prefix.size()] != '/')
            continue;

        if (best == NULL || prefix.size() > best->path.size())
            best = &location;
    }

    return best;
}

bool isPostAllowed(const ServerConfig& config, const LocationConfig& location)
{
    if (!location.allowed_methods.empty())
    {
        for (size_t i = 0; i < location.allowed_methods.size(); ++i)
        {
            if (location.allowed_methods[i] == "POST")
                return true;
        }

        return false;
    }

    if (!config.allowed_methods.empty())
    {
        for (size_t i = 0; i < config.allowed_methods.size(); ++i)
        {
            if (config.allowed_methods[i] == POST)
                return true;
        }

        return false;
    }

    // No explicit permission configured.
    return false;
}

bool getUploadFilename(const std::string& target, const LocationConfig& location, std::string& filename)
{
    size_t queryPos = target.find('?');
    std::string path = target.substr(0, queryPos);

    const std::string& prefix = location.path;

    if (path.compare(0, prefix.size(), prefix) != 0)
        return false;

    // Remove the location prefix.
    filename = path.substr(prefix.size());

    // Remove the leading slash, if present.
    if (!filename.empty() && filename[0] == '/')
        filename.erase(0, 1);

    // Only allow a single filename, not a path.
    if (filename.empty() ||
        filename == "." ||
        filename == ".." ||
        filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos)
        return false;

    return true;
}

int openUploadDirectory(const LocationConfig& location)
{
    return open(location.root.c_str(), O_RDONLY | O_DIRECTORY);
}

int openUploadFile(int dirFd, const std::string& filename, bool& existed)
{
    struct stat info;

    if (fstatat(dirFd, filename.c_str(), &info,
                AT_SYMLINK_NOFOLLOW) == 0)
    {
        if (!S_ISREG(info.st_mode))
        {
            errno = EACCES;
            return -1;
        }

        existed = true;
    }
    else
    {
        if (errno != ENOENT)
            return -1;

        existed = false;
    }

    return openat(
        dirFd,
        filename.c_str(),
        O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW,
        0644
    );
}

bool writeRequestBody(int fileFd, const std::string& body)
{
    size_t written = 0;

    while (written < body.size())
    {
        ssize_t result = write(
            fileFd,
            body.data() + written,
            body.size() - written
        );

        if (result <= 0) //removed errno if statement as that is forbidden for server behavior
            return false;

        written += static_cast<size_t>(result);
    }

    return true;
}

int saveUploadedFile(const LocationConfig& location, const std::string& filename, const std::string& body)
{
    int dirFd = openUploadDirectory(location);

    if (dirFd == -1)
    {
        std::cerr << "DEBUG: openUploadDirectory failed: "
                << location.root << '\n';
        return 500;
    }

    bool existed = false;
    int fileFd = openUploadFile(dirFd, filename, existed);

    if (fileFd == -1)
    {
        std::cerr << "DEBUG: openUploadFile failed for: "
              << filename << '\n';
        int savedErrno = errno;
        close(dirFd);

        if (savedErrno == EACCES || savedErrno == ELOOP)
            return 403;

        if (savedErrno == ENOENT || savedErrno == ENOTDIR)
            return 404;

        return 500;
    }

    std::cerr << "DEBUG: opened file successfully, body size = "
          << body.size() << '\n';

    bool success = writeRequestBody(fileFd, body);

    if (!success)
        std::cerr << "DEBUG: writeRequestBody failed\n";

    if (close(fileFd) == -1)
        success = false;

    close(dirFd);

    if (!success)
        return 500;

    return existed ? 200 : 201;
}

bool sendPostResponse(int clientFd, const std::string& version, bool keepAlive, int status)
{
    std::string reason;

    switch (status)
    {
        case 200: reason = "OK"; break;
        case 201: reason = "Created"; break;
        case 400: reason = "Bad Request"; break;
        case 403: reason = "Forbidden"; break;
        case 404: reason = "Not Found"; break;
        case 405: reason = "Method Not Allowed"; break;
        default:  reason = "Internal Server Error"; break;
    }

    std::ostringstream response;

    response << version << " " << status << " " << reason << "\r\n";
    response << "Content-Length: 0\r\n";
    response << (keepAlive
        ? "Connection: keep-alive\r\n"
        : "Connection: close\r\n");
    response << "\r\n";

    std::string output = response.str();

    return sendAll(clientFd, output.data(), output.size()) != -1;
}

bool handlePost(const HttpParser& http, const std::string& version, bool keepAlive, int clientFd, const ServerConfig& config)
{
    const LocationConfig* location =
        findMatchingLocation(config, http.getTarget());

    if (location == NULL)
    {
        std::cout << "POST failed: no matching location\n";
        return sendPostResponse(clientFd, version, keepAlive, 404);
    }

    if (!isMethodAllowed(config, location, "POST"))
    {
        std::cout << "POST failed: method not allowed\n";
        return sendPostResponse(clientFd, version, keepAlive, 405);
    }

    std::string filename;

    if (!getUploadFilename(http.getTarget(), *location, filename))
    {
        std::cout << "POST failed: could not get upload filename\n";
        return sendPostResponse(clientFd, version, keepAlive, 400);
    }

    int status = saveUploadedFile(*location, filename, http.getBody());

    return sendPostResponse(clientFd, version, keepAlive, status);
}