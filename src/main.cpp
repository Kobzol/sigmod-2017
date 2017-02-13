#include <iostream>
#include <vector>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <chrono>

#include "settings.h"
#include "word.h"
#include "query.h"
#include "util.h"
#include "nfa.h"
#include "job.h"
#include "thread.h"

static std::unordered_map<std::string, std::vector<int>> prefixMap;
static Nfa<std::string> nfa;

#ifndef SOLVE_ON_MAIN_THREAD
    static ThreadPool threadPool;
    extern JobQueue jobQueue;
#endif

static std::vector<Word> ngrams;
std::vector<Query> queries;

void load_init_data(std::istream& input)
{
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
            ngrams.emplace_back(line, 0);
        }
    }
}

void find_in_document(Query& query)
{
    std::string prefix;
    std::vector<Match> matches;

    size_t timestamp = query.timestamp;
    std::string& line = query.document;
    line += ' ';

    NfaVisitor visitor;

    for (size_t i = 0; i < line.size(); i++)
    {
        char c = line.at(i);
        if (c == ' ')
        {
            std::vector<ssize_t> indices;
            nfa.feedWord(visitor, prefix, indices);
            for (ssize_t index : indices)
            {
                if (ngrams.at(index).is_active(timestamp))
                {
                    const std::string& word = ngrams.at(index).word;
                    matches.emplace_back(i - word.size(), word);
                }
            }

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
        query.result += "-1";
    }
    else
    {
        found.insert(matches.at(0).word);
        query.result += matches.at(0).word;
    }

    for (size_t i = 1; i < matches.size(); i++)
    {
        Match& match = matches.at(i);
        if (!found.count(match.word))
        {
            query.result += '|';
            query.result += match.word;
            found.insert(match.word);
        }
    }
}

int main()
{
    std::ios::sync_with_stdio(false);

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
    int additions = 0, deletions = 0, query_count = 0, init_ngrams = 0;
    size_t query_length = 0, ngram_length = 0;
#endif

    init_linear_map();

    load_init_data(input);

    for (size_t i = 0; i < ngrams.size(); i++)
    {
        std::string prefix = find_prefix(ngrams.at(i).word);
        if (!prefixMap.count(prefix))
        {
            prefixMap[prefix] = std::vector<int>();
        }
        prefixMap.at(prefix).emplace_back(i);

        nfa.addWord(ngrams.at(i), i);

#ifdef PRINT_STATISTICS
        init_ngrams++;
        ngram_length += ngrams.at(i).word.size();
#endif
    }

    size_t timestamp = 0;

    ngrams.reserve(1000 * 1000);
    queries.reserve(100 * 1000);

    std::cout << "R" << std::endl;

    while (true)
    {
        std::string line;
        if (!std::getline(input, line) || line.size() == 0) break;
        char op = line[0];
        timestamp++;

        if (op == 'A')
        {
            line = line.substr(2);

            ngrams.emplace_back(line, timestamp);
            std::string prefix = find_prefix(line);
            if (!prefixMap.count(prefix))
            {
                prefixMap[prefix] = std::vector<int>();
            }
            prefixMap.at(prefix).emplace_back(ngrams.size() - 1);

            nfa.addWord(ngrams.at(ngrams.size() - 1), ngrams.size() - 1);

#ifdef PRINT_STATISTICS
        additions++;
        ngram_length += line.size();
#endif
        }
        else if (op == 'D')
        {
            line = line.substr(2);

            // TODO: try linear search
            std::string prefix = find_prefix(line);

            if (prefixMap.count(prefix))
            {
                for (int i : prefixMap.at(prefix))
                {
                    Word& ngram = ngrams.at(i);
                    if (ngram.is_active(timestamp) && ngram.word == line)
                    {
                        ngram.to = timestamp;
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

            queries.emplace_back(line, timestamp);

#ifdef SOLVE_ON_MAIN_THREAD
            find_in_document(queries.at(queries.size() - 1));
#else
            jobQueue.add_query(queries.size() - 1);
#endif

#ifdef PRINT_STATISTICS
            query_count++;
            query_length += line.size();
#endif
        }
        else
        {
#ifdef SOLVE_ON_MAIN_THREAD
            for (auto& query : queries)
            {
                std::cout << query.result << std::endl;
            }
#else
            for (Query& query : queries)
            {
                while (!query.jobFinished)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                std::cout << query.result << std::endl;
            }
#endif

            queries.clear();

            std::cout << std::flush;
        }
    }

#ifdef PRINT_STATISTICS
    std::cerr << "Initial ngrams: " << init_ngrams << std::endl;
    std::cerr << "Additions: " << additions << std::endl;
    std::cerr << "Deletions: " << deletions << std::endl;
    std::cerr << "Queries: " << query_count << std::endl;
    std::cerr << "Average query length: " << query_length / query_count << std::endl;
    std::cerr << "Average ngram length: " << ngram_length / ngrams.size() << std::endl;

    std::cerr << "State count: " << nfa.states.size() << std::endl;
    size_t stateSum = ((HashNfaState<std::string>*)nfa.states.at(0))->arcs.size();
    std::cerr << "Hash state size: " << (double) stateSum  << std::endl;

#endif

    return 0;
}
