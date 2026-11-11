#include "Configuration.hpp"

void parse(std::vector<ServerConfig>& servers, const std::vector<Token>& tokens)
{
    size_t pos = 0;

    while (pos < tokens.size())
    {
        if (tokens[pos].word != "server")
            throw std::runtime_error("expected 'server', got '" + tokens[pos].word + "'");
        servers.push_back(parse_server(tokens, pos));
    }
}
