#include "Responder.hpp"
#include <algorithm>
#include <cctype>

std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

std::string toUpper(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(value[i])));
    return value;
}

std::string statusText(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Content Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Method Not Implemented";
        case 505: return "HTTP Version Not Supported";
        default: return "";
    }
}

std::string allowHeaderValue(const ServerConfig& config, const LocationConfig* location)
{
    std::vector<std::string> methods;
    if (location != NULL && !location->allowed_methods.empty())
        methods = location->allowed_methods;
    else if (!config.allowed_methods.empty())
    {
        for (size_t i = 0; i < config.allowed_methods.size(); ++i)
        {
            if (config.allowed_methods[i] == GET)
                methods.push_back("GET");
            else if (config.allowed_methods[i] == POST)
                methods.push_back("POST");
            else if (config.allowed_methods[i] == DELETE)
                methods.push_back("DELETE");
        }
    }
    else
        methods.push_back("GET");

    std::string value;
    for (size_t i = 0; i < methods.size(); ++i)
    {
        if (!value.empty())
            value += ", ";
        value += methods[i];
    }
    return value;
}

std::string findHeaderValue(const std::map<std::string, std::string>& headers, const std::string& name)
{
    std::string lname = toLower(name);
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it)
        if (toLower(it->first) == lname)
            return it->second;
    return std::string();
}

std::string normalizeTarget(const std::string& rawTarget)
{
    std::string target = rawTarget;
    const size_t qpos = target.find('?');
    const size_t hashPos = target.find('#');

    if (qpos != std::string::npos)
        target = target.substr(0, qpos);
    if (hashPos != std::string::npos)
        target = target.substr(0, hashPos);
    if (target.empty() || target[0] != '/')
        target = "/" + target;
    return target;
}

std::string resolveRequestedPath(const std::string& root, const std::string& target)
{
    std::string baseRoot = root;
    if (baseRoot.empty())
        baseRoot = ".";
    if (!baseRoot.empty() && baseRoot.back() == '/')
        baseRoot.pop_back();

    std::string path = baseRoot + target;
    if (!path.empty() && path.back() == '/')
        path += "index.html";
    return path;
}
