#include "Configuration.hpp"

std::vector<Token> lex(const std::string& text)
{
    std::vector<Token> tokens;
    size_t j;
    size_t i;
    size_t line = 1;

    i = 0;
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

void expect_and_increase(const std::vector<Token>& tokens, size_t& pos, CONF_TOKEN type)
{
    if (pos >= tokens.size() || tokens[pos].type != type)
        throw std::runtime_error("unexpected token");
    pos++;
}
