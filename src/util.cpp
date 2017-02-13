#include "util.h"

std::string find_prefix_from(const std::string& word, size_t from)
{
    auto pos = word.find(' ', from);

    if (pos == std::string::npos)
    {
        return word.substr(from);
    }
    else return word.substr(from, pos - from);
}

std::string find_prefix(const std::string& word)
{
    auto pos = word.find(' ');

    if (pos == std::string::npos)
    {
        return word;
    }
    else return word.substr(0, pos);
}