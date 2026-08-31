#include "Configuration.hpp"
#include <iostream>

void parse_server_directive(const std::vector<Token>& tokens, size_t& pos, ServerConfig& server)
{
    std::string value;
    std::string keyword = tokens[pos].word;
    pos++;
        // more values here too
    expect(tokens, pos, WORD);
    value = tokens[pos - 1].word;
    expect(tokens, pos, SEMICOLON);

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
        expect(tokens, pos, WORD);
        location.root = tokens[pos - 1].word;
        expect(tokens, pos, SEMICOLON);
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

    expect(tokens, pos, WORD);
    expect(tokens, pos, WORD);
    location.path = tokens[pos - 1].word;
    expect(tokens, pos, LEFTBRACE);
    // note 
    while (pos < tokens.size() && tokens[pos].type != RIGHTBRACE)
        parse_location_directive(tokens, pos, location);
    expect(tokens, pos, RIGHTBRACE);
    return location;
}

ServerConfig parse_server(const std::vector<Token>& tokens, size_t& pos)
{
    ServerConfig server;

    expect(tokens, pos, WORD);
    expect(tokens, pos, LEFTBRACE);
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
                expect(tokens, pos, WORD);
                server.allowed_methods.push_back(tokens[pos - 1].word);
            }
            expect(tokens, pos, SEMICOLON);
        }
        else
            parse_server_directive(tokens, pos, server);
    }
    expect(tokens, pos, RIGHTBRACE);
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

std::vector<Token> lex(const std::string& text)
{
    std::vector<Token> tokens;
    size_t j, i = 0;
    size_t line = 1;

    while (i < text.size())
    {
        if (text[i] == '#') // hehe
            while (++i < text.size() && text[i] != '\n');
        if (text[i] == '\n')
        {
            line++;
            i++;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(text[i])))
        {
            i++;
            continue;
        }
        if (text[i] == '{')
        {
            tokens.push_back({LEFTBRACE, "{", line});
            i++;
            continue;
        }
        if (text[i] == '}')
        {
            tokens.push_back({RIGHTBRACE, "}", line});
            i++;
            continue;
        }
        if (text[i] == ';')
        {
            tokens.push_back({SEMICOLON, ";", line});
            i++;
            continue;
        }
        j = i;
        while (j < text.size() && !std::isspace(static_cast<unsigned char>(text[j]))
            && text[j] != '{' && text[j] != '}' && text[j] != ';')
            j++;
        tokens.push_back({WORD, text.substr(i, j - i), line});
        i = j;
    }

    return tokens;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./parser config.conf\n";
        return 1;
    }
    std::string conf = argv[1];
    std::ifstream file(conf);
    std::stringstream buffer;
    std::string config;

    if (!file || conf.size() < 5 || 
        conf.compare(conf.size() - 5, 5, ".conf") != 0)
    {
        std::cout << "Needs to be .conf\n";
        return 1;
    }

    buffer << file.rdbuf();
    config = buffer.str();
    
    std::cout << "-------------------LEX AND TOK-----------\n";
    std::vector<Token> lexxer = lex(config);
    for (long unsigned int i = 0; i < lexxer.size(); i++)
        std::cout << lexxer[i].type  << "  " << lexxer[i].word << "\n";
    //parse
    std::vector<ServerConfig> servers;
    parse(servers, lexxer);

    return 0;
}