#pragma once

#include <string>

std::string find_prefix(const std::string& word, size_t start_index = 0)
{
    auto pos = word.find(' ', start_index);

    if (pos == std::string::npos)
    {
        return word;
    }
    else return word.substr(start_index, pos);
}
size_t find_prefix_length(const std::string& word, size_t start_index = 0)
{
    size_t length = 0;
    for (; start_index < word.size(); start_index++)
    {
        if (word.at(start_index) == ' ') break;
        length++;
    }

    return length;
}
