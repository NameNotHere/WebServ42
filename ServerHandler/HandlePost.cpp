#include "Server.hpp"

const LocationConfig* findMatchingLocation(const ServerConfig& config, const std::string& target, const std::string& method)
{
    size_t queryPos = target.find('?');
    std::string path = target.substr(0, queryPos);

    const LocationConfig* bestPrefix = NULL;
    const LocationConfig* bestExtension = NULL;
    size_t bestExtensionLength = 0;

    for (size_t i = 0; i < config.locations.size(); ++i)
    {
        const LocationConfig& location = config.locations[i];
        const std::string& prefix = location.path;

        if (prefix.empty())
            continue;

        if (prefix.size() > 2 && prefix.compare(0, 2, "*.") == 0)
        {
            const std::string extension = prefix.substr(1);
            if (path.size() >= extension.size()
                && path.compare(path.size() - extension.size(),
                                extension.size(), extension) == 0
                && (bestExtension == NULL
                    || extension.size() > bestExtensionLength))
            {
                bestExtension = &location;
                bestExtensionLength = extension.size();
            }
            continue;
        }

        if (path.compare(0, prefix.size(), prefix) != 0)
            continue;

        // Avoid matching /upload when the path is /upload2.
        if (prefix[prefix.size() - 1] != '/' &&
            path.size() > prefix.size() &&
            path[prefix.size()] != '/')
            continue;

        if (bestPrefix == NULL || prefix.size() > bestPrefix->path.size())
            bestPrefix = &location;
    }

    if (bestExtension != NULL
        && isMethodAllowed(config, bestExtension, method))
        return bestExtension;
    if (bestPrefix != NULL
        && isMethodAllowed(config, bestPrefix, method))
        return bestPrefix;
    return bestExtension != NULL ? bestExtension : bestPrefix;
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

    if (filename.empty() && !location.uploadDir.empty())
    {
        std::string endpoint = prefix;
        while (!endpoint.empty() && endpoint[endpoint.size() - 1] == '/')
            endpoint.erase(endpoint.size() - 1);

        size_t slash = endpoint.find_last_of('/');
        filename = slash == std::string::npos
            ? endpoint : endpoint.substr(slash + 1);
    }

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
    const std::string& directory =
        location.uploadDir.empty() ? location.root : location.uploadDir;

    return open(directory.c_str(), O_RDONLY | O_DIRECTORY);
}

int openUploadFile(int dirFd, const std::string& filename, bool& existed)
{
    const std::string filePath = "/proc/self/fd/"
        + std::to_string(dirFd) + "/" + filename;
    struct stat info;

    if (stat(filePath.c_str(), &info) == 0)
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

    return open(
        filePath.c_str(),
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
        findMatchingLocation(config, http.getTarget(), "POST");

    if (location == NULL)
    {
        return sendErrorResponse(clientFd, version, keepAlive, 404, &config);
    }

    if (!isMethodAllowed(config, location, "POST"))
    {
        return sendErrorResponse(
            clientFd, version, keepAlive, 405, &config,
            std::vector<std::pair<std::string, std::string> >(1,
                std::make_pair("Allow", allowHeaderValue(config, location))));
    }

    std::string filename;

    if (!getUploadFilename(http.getTarget(), *location, filename))
    {
        return sendErrorResponse(clientFd, version, keepAlive, 400, &config);
    }

    int status = saveUploadedFile(*location, filename, http.getBody());

    if (status >= 400)
        return sendErrorResponse(clientFd, version, keepAlive, status, &config);
    return sendPostResponse(clientFd, version, keepAlive, status);
}
