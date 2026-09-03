#include "Configuration.hpp"
#include <iostream>

void parse_server_directive(const std::vector<Token>& tokens, size_t& pos, ServerConfig& server)
{
    std::string value;
    std::string keyword = tokens[pos].word;
    pos++;
        // more values here too
    expect_and_increase(tokens, pos, WORD);
    value = tokens[pos - 1].word;
    expect_and_increase(tokens, pos, SEMICOLON);

    if (keyword == "listen")
        server.listen = std::stoi(value);
    else if (keyword == "root")
        server.root = value;
    else
        throw std::runtime_error("unknown directive: " + keyword);
}

void parse_location_directive(const std::vector<Token> &tokens, size_t &pos, LocationConfig &location)
{ // need to expand this to accept more hahaha 
    std::string keyword = tokens[pos].word;
    pos++;

    if (keyword == "root")
    {
        expect_and_increase(tokens, pos, WORD);
        location.root = tokens[pos - 1].word;
        expect_and_increase(tokens, pos, SEMICOLON);
    }
    else
    {
        throw std::runtime_error(
            "unknown location directive: " + keyword);
    }
}

LocationConfig parse_location(
    const std::vector<Token>& tokens,
    size_t& pos)
{
    LocationConfig location;

    expect_and_increase(tokens, pos, WORD);
    expect_and_increase(tokens, pos, WORD);
    location.path = tokens[pos - 1].word;
    expect_and_increase(tokens, pos, LEFTBRACE);
    while (pos < tokens.size() && tokens[pos].type != RIGHTBRACE)
        parse_location_directive(tokens, pos, location);
    expect_and_increase(tokens, pos, RIGHTBRACE);
    return location;
}

ServerConfig parse_server(const std::vector<Token>& tokens, size_t& pos)
{
    ServerConfig server;

    expect_and_increase(tokens, pos, WORD);
    expect_and_increase(tokens, pos, LEFTBRACE);
    while (tokens[pos].type != RIGHTBRACE && pos <= tokens.size())    {
        if (tokens[pos].word == "location")
        {
            LocationConfig location = parse_location(tokens, pos);
            server.locations.push_back(location);
        }
        else if (tokens[pos].word == "allow_methods")
        {
            while (pos < tokens.size() &&
                   tokens[pos].type != SEMICOLON)
            {
                expect_and_increase(tokens, pos, WORD);
                server.allowed_methods.push_back(tokens[pos - 1].word);
            }
            expect_and_increase(tokens, pos, SEMICOLON);
        }
        else
            parse_server_directive(tokens, pos, server);
    }
    expect_and_increase(tokens, pos, RIGHTBRACE);
    return server;
}

void parse(std::vector<ServerConfig> &servers, const std::vector<Token>& tokens)
{
    size_t i = 0;

    while (i < tokens.size())
    {
        if (tokens[i].word == "server")
            servers.push_back(parse_server(tokens, i));
        else
            throw std::runtime_error("expected 'server'");
    }
}
