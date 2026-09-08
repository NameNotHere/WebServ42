#include "Configuration.hpp"
#include <iostream>
#include <cctype>

std::vector<Token> lex(const std::string& text)
{
    std::vector<Token> tokens;

    size_t i = 0;
    size_t j = 0;
    size_t line = 1;

    while (i < text.size())
    {
        if (std::isspace(static_cast<unsigned char>(text[i])))
        {
            if (text[i] == '\n')
                line++;

            i++;
            continue;
        }
        if (text[i] == '#')
        {
            while (i < text.size() && text[i] != '\n')
                i++;
            continue;
        }
        if (text[i] == '{')
        {
            tokens.push_back(Token{LEFTBRACE, "{", line});
            i++;
            continue;
        }
        if (text[i] == '}')
        {
            tokens.push_back(Token{RIGHTBRACE, "}", line});
            i++;
            continue;
        }
        if (text[i] == ';')
        {
            tokens.push_back(Token{SEMICOLON, ";", line});
            i++;
            continue;
        }
        
        j = i;
        while (j < text.size() && 
        !std::isspace(static_cast<unsigned char>(text[j])) 
        && text[j] != '{' && text[j] != '}' && text[j] != ';')
            j++;
        tokens.push_back(Token{WORD, text.substr(i, j - i), line});
        i = j;
    }

    return tokens;
}

void expect_and_increase(const std::vector<Token>& tokens,
                         size_t& pos,
                         CONF_TOKEN expected)
{
    if (pos >= tokens.size())
        throw std::runtime_error("unexpected end of file");

    std::cerr << "Expected: " << expected
              << " | Got: " << tokens[pos].type
              << " | word: [" << tokens[pos].word << "]"
              << " | pos: " << pos
              << std::endl;

    if (tokens[pos].type != expected)
        throw std::runtime_error(
            "unexpected token at line " + tokens[pos].line
            + std::to_string(tokens[pos].line)
        );

    pos++;
}
