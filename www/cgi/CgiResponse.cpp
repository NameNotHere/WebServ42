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

    bool splitCgiResponse(const std::string& raw, std::string& headerBlock,
                          std::string& body)
    {
        size_t separator = raw.find("\r\n\r\n");
        size_t separatorLength = 4;
        if (separator == std::string::npos)
        {
            separator = raw.find("\n\n");
            separatorLength = 2;
            if (separator == std::string::npos)
                return false;
        }
        headerBlock = raw.substr(0, separator);
        body = raw.substr(separator + separatorLength);
        return true;
    }

    bool parseStatusValue(const std::string& value, CgiResponse& response)
    {
        std::istringstream stream(value);
        std::string code;
        stream >> code;
        if (code.size() != 3
            || !std::isdigit(static_cast<unsigned char>(code[0]))
            || !std::isdigit(static_cast<unsigned char>(code[1]))
            || !std::isdigit(static_cast<unsigned char>(code[2])))
            return false;

        response.status_code = (code[0] - '0') * 100
            + (code[1] - '0') * 10 + code[2] - '0';
        std::getline(stream, response.status_message);
        if (!response.status_message.empty()
            && response.status_message[0] == ' ')
            response.status_message = response.status_message.substr(1);
        return true;
    }

    bool parseCgiHeaderLine(std::string line, CgiResponse& response,
                            bool& statusSeen)
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty())
            return false;

        size_t colon = line.find(':');
        if (colon == std::string::npos)
            return false;
        std::string name = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        if (!validHeaderName(name))
            return false;

        size_t first = value.find_first_not_of(" \t");
        if (first != std::string::npos)
            value = value.substr(first);
        size_t last = value.find_last_not_of(" \t");
        if (last != std::string::npos)
            value = value.substr(0, last + 1);
        if (!validHeaderValue(value))
            return false;

        for (size_t i = 0; i < name.size(); ++i)
            name[i] = static_cast<char>(
                std::tolower(static_cast<unsigned char>(name[i])));

        if (name == "status")
        {
            if (statusSeen)
                return false;
            statusSeen = true;
            return parseStatusValue(value, response);
        }
        if (response.headers.find(name) != response.headers.end())
            return false;
        response.headers[name] = value;
        return true;
    }

    bool hasRequiredCgiHeader(const CgiResponse& response)
    {
        return response.headers.find("content-type") != response.headers.end()
            || response.headers.find("location") != response.headers.end();
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

    std::string headerBlock;
    if (!splitCgiResponse(raw, headerBlock, body))
        return false;

    bool statusSeen = false;
    std::istringstream stream(headerBlock);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!parseCgiHeaderLine(line, *this, statusSeen))
            return false;
    }

    return hasRequiredCgiHeader(*this);
}
