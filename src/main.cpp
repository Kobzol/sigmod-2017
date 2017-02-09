#include <iostream>
#include <vector>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <sstream>

#include "settings.h"
#include "word.h"

static int query_id = 0;

std::vector<Word> load_init_data(std::istream& input)
{
    std::vector<Word> ngrams;

    while (true)
    {
        std::string line;
        std::getline(input, line);

        if (line == "S")
        {
            break;
        }
        else
        {
            ngrams.emplace_back(line);
        }
    }

    return ngrams;
}

std::string find_prefix(std::string word)
{
    auto pos = word.find(' ');

    if (pos == std::string::npos)
    {
        return word;
    }
    else return word.substr(0, pos);
}

void find_in_document(std::string line, const std::vector<Word>& ngrams, const std::unordered_map<std::string, std::vector<int>>& prefixMap)
{
    std::string prefix;
    size_t start = 0;

    std::vector<Match> matches;

    line += ' ';

    for (size_t i = 0; i < line.size(); i++)
    {
        char c = line.at(i);
        if (c == ' ')
        {
            if (prefixMap.count(prefix))
            {
                for (int i : prefixMap.at(prefix))
                {
                    const Word& ngram = ngrams.at(i);

                    if (!ngram.active) continue;

                    size_t length = ngram.word.size();
                    size_t j = 0;
                    for (; j < length && start + j < line.size(); j++)
                    {
                        if (ngram.word.at(j) != line.at(start + j))
                        {
                            break;
                        }
                    }

                    if (j == length && (start + ngram.word.size() == line.size() || line[start + ngram.word.size()] == ' '))
                    {
                        matches.emplace_back(start, ngram.word);
                    }
                }
            }

            start = i + 1;
            prefix = "";
        }
        else
        {
            prefix += c;
        }
    }

    std::sort(matches.begin(), matches.end(), [](Match& m1, Match& m2)
    {
        if (m1.index < m2.index) return true;
        else if (m1.index > m2.index) return false;
        else return m1.word.size() <= m2.word.size();
    });

    std::unordered_set<std::string> found;
    if (matches.size() == 0)
    {
        std::cout << "-1";
    }
    else
    {
        found.insert(matches.at(0).word);
        std::cout << matches.at(0).word;
    }

    for (size_t i = 1; i < matches.size(); i++)
    {
        Match& match = matches.at(i);
        if (!found.count(match.word))
        {
            std::cout << "|" << match.word;
            found.insert(match.word);
        }
    }

    query_id++;
}

int main()
{
#ifdef LOAD_FROM_FILE
    std::fstream file(LOAD_FROM_FILE, std::iostream::in);

    if (!file.is_open())
    {
        throw "File not found";
    }

    std::istream& input = file;
#else
    std::istream& input = std::cin;
#endif

#ifdef PRINT_STATISTICS
    int additions = 0, deletions = 0, queries = 0, init_ngrams = 0;
    size_t query_length = 0, ngram_length = 0;
#endif

    std::vector<Word> ngrams = load_init_data(input);
    std::unordered_map<std::string, std::vector<int>> prefixMap;

    for (size_t i = 0; i < ngrams.size(); i++)
    {
        std::string prefix = find_prefix(ngrams.at(i).word);
        if (!prefixMap.count(prefix))
        {
            prefixMap[prefix] = std::vector<int>();
        }
        prefixMap.at(prefix).emplace_back(i);

#ifdef PRINT_STATISTICS
        init_ngrams++;
        ngram_length += ngrams.at(i).word.size();
#endif
    }

    std::cout << "R" << std::endl;

    while (true)
    {
        std::string line;
        if (!std::getline(input, line) || line.size() == 0) break;
        char op = line[0];

        if (op == 'A')
        {
            line = line.substr(2);

            ngrams.emplace_back(line);
            std::string prefix = find_prefix(line);
            if (!prefixMap.count(prefix))
            {
                prefixMap[prefix] = std::vector<int>();
            }
            prefixMap.at(prefix).emplace_back(ngrams.size() - 1);

#ifdef PRINT_STATISTICS
        additions++;
        ngram_length += line.size();
#endif
        }
        else if (op == 'D')
        {
            line = line.substr(2);

            std::string prefix = find_prefix(line);

            if (prefixMap.count(prefix))
            {
                for (int i : prefixMap.at(prefix))
                {
                    Word& ngram = ngrams.at(i);
                    if (ngram.active && ngram.word == line)
                    {
                        ngram.active = false;
                    }
                }
            }

#ifdef PRINT_STATISTICS
            deletions++;
#endif
        }
        else if (op == 'Q')
        {
            line = line.substr(2);

            find_in_document(line, ngrams, prefixMap);

            std::cout << std::endl;

#ifdef PRINT_STATISTICS
            queries++;
            query_length += line.size();
#endif
        }
        else std::cout << std::flush;
    }

#ifdef PRINT_STATISTICS
    std::cerr << "Initial ngrams: " << init_ngrams << std::endl;
    std::cerr << "Additions: " << additions << std::endl;
    std::cerr << "Deletions: " << deletions << std::endl;
    std::cerr << "Queries: " << queries << std::endl;
    std::cerr << "Average query length: " << query_length / queries << std::endl;
    std::cerr << "Average ngram length: " << ngram_length / ngrams.size() << std::endl;
#endif

    return 0;
}
