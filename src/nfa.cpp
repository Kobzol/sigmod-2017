#include "nfa.h"

// TODO: unroll
int get_prefix(const std::string& word, const size_t start, const std::string& needle)
{
    int end = (int) std::min(needle.size(), word.size() - start);
    int prefix = 0;
    for (; prefix < end; prefix++)
    {
        if (word[start + prefix] != needle[prefix]) return prefix;
    }
    return prefix;
}
int get_prefix(const std::string& word, size_t wordStart, const std::string& needle, size_t needleStart)
{
    int end = (int) std::min(needle.size() - needleStart, word.size() - wordStart);
    int prefix = 0;
    for (; prefix < end; prefix++)
    {
        if (word[wordStart + prefix] != needle[needleStart + prefix]) return prefix;
    }
    return prefix;
}

