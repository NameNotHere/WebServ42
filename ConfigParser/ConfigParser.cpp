#include "Configuration.hpp"
#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <vector>

void parse_server_directive(const std::vector<Token>& tokens, size_t& pos, ServerConfig& server)
{
    int code;
    std::string keyword = tokens[pos].word;
    
    pos++;
    if (keyword == "listen")
    {
        expect_and_increase(tokens, pos, WORD);
        server.listen = static_cast<unsigned int>(std::stoi(tokens[pos - 1].word));
    }
    else if (keyword == "root")
    {
        expect_and_increase(tokens, pos, WORD);
        server.root = tokens[pos - 1].word;
    }
    else if (keyword == "err")
    {
        expect_and_increase(tokens, pos, WORD);
        code = std::stoi(tokens[pos - 1].word);
        expect_and_increase(tokens, pos, WORD);
        server.errors[code] = tokens[pos - 1].word;
    }
    else
        throw std::runtime_error("unknown server directive: " + keyword);
    expect_and_increase(tokens, pos, SEMICOLON);
}

void parse_location_directive(const std::vector<Token>& tokens, size_t& pos, LocationConfig& location)
{
    if (pos >= tokens.size())
        throw std::runtime_error("unexpected end of file");

    std::string keyword = tokens[pos].word;
    pos++;

    if (keyword == "root")
    {
        expect_and_increase(tokens, pos, WORD);
        location.root = tokens[pos - 1].word;
        expect_and_increase(tokens, pos, SEMICOLON);
    }
    else if (keyword == "allowed_methods")
    {
        while (pos < tokens.size() && tokens[pos].type != SEMICOLON)
        {
            expect_and_increase(tokens, pos, WORD);
            location.allowed_methods.push_back(tokens[pos - 1].word);
        }
        expect_and_increase(tokens, pos, SEMICOLON);
    }
    else
        throw std::runtime_error("unknown location directive: " + keyword);
}

LocationConfig parse_location(const std::vector<Token>& tokens, size_t& pos)
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

ALLOWED_METHODS parse_method(const std::string& method)
{
    if (method == "GET")
        return GET;
    if (method == "POST")
        return POST;
    if (method == "DELETE")
        return DELETE;
    throw std::runtime_error("unknown HTTP method: " + method);
}

ServerConfig parse_server(const std::vector<Token>& tokens, size_t& pos)
{
    ServerConfig server;
    LocationConfig location;

    // server
    expect_and_increase(tokens, pos, WORD);
    if (pos < tokens.size() && tokens[pos].type == WORD)
        server.name = tokens[pos++].word;
    expect_and_increase(tokens, pos, LEFTBRACE);

    while (pos < tokens.size() && tokens[pos].type != RIGHTBRACE)
    {
        if (tokens[pos].word == "location")
        {
            location = parse_location(tokens, pos);
            server.locations.push_back(location);
        }
        else if (tokens[pos].word == "allowed_methods")
        {
            pos++;
            while (pos < tokens.size() && tokens[pos].type != SEMICOLON)
            {
                expect_and_increase(tokens, pos, WORD);
                server.allowed_methods.push_back(parse_method(tokens[pos - 1].word));
            }
            expect_and_increase(tokens, pos, SEMICOLON);
        }
        else
            parse_server_directive(tokens, pos, server);
    }
    expect_and_increase(tokens, pos, RIGHTBRACE);

    return server;
}

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
