#include "CgiResponse.hpp"

#include <sstream>
#include <cctype>

namespace
{
    bool validHeaderName(const std::string& name)
    {
        if (name.empty())
            return false;
        for (size_t i = 0; i < name.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(name[i]);
            if (!std::isalnum(c) && std::string("!#$%&'*+-.^_`|~").find(c) == std::string::npos)
                return false;
        }
        return true;
    }
    bool validHeaderValue(const std::string& value)
    {
        for (size_t i = 0; i < value.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(value[i]);
            if ((c < 0x20 && c != '\t') || c == 0x7f)
                return false;
        }
        return true;
    }
}

CgiResponse::CgiResponse()
    : status_code(200), status_message("OK")
{
}

bool CgiResponse::parse(const std::string& raw)
{
    status_code = 200;
    status_message = "OK";
    headers.clear();
    body.clear();
    bool statusSeen = false;
    size_t sep = raw.find("\r\n\r\n");
    size_t sepLen = 4;

    if (sep == std::string::npos)
    {
        sep = raw.find("\n\n");
        sepLen = 2;
        if (sep == std::string::npos)
            return false;
    }

    std::string headerBlock = raw.substr(0, sep);
    body                    = raw.substr(sep + sepLen);
    std::istringstream stream(headerBlock);
    std::string line;

    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.empty())
            return false;

        size_t colon = line.find(':');
        if (colon == std::string::npos)
            return false;

        std::string name  = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        if (!validHeaderName(name))
            return false;
        size_t s = value.find_first_not_of(" \t");
        if (s != std::string::npos)
            value = value.substr(s);
        size_t e = value.find_last_not_of(" \t");
        if (e != std::string::npos)
            value = value.substr(0, e + 1);
        if (!validHeaderValue(value))
            return false;

        // Lowercase the header name for consistent lookups
        for (size_t i = 0; i < name.size(); ++i)
            name[i] = static_cast<char>(
                std::tolower(static_cast<unsigned char>(name[i])));

        if (name == "status")
        {
            if (statusSeen)
                return false;
            statusSeen = true;
            std::istringstream ss(value);
            std::string code;
            ss >> code;
            if (code.size() != 3 || !std::isdigit(static_cast<unsigned char>(code[0]))
                || !std::isdigit(static_cast<unsigned char>(code[1]))
                || !std::isdigit(static_cast<unsigned char>(code[2])))
                return false;
            status_code = (code[0] - '0') * 100 + (code[1] - '0') * 10 + code[2] - '0';
            std::getline(ss, status_message);
            if (!status_message.empty() && status_message[0] == ' ')
                status_message = status_message.substr(1);
        }
        else
        {
            if (headers.find(name) != headers.end())
                return false;
            headers[name] = value;
        }
    }
    return headers.find("content-type") != headers.end()
        || headers.find("location") != headers.end();
}