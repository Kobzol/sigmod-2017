#include <iostream>
#include <vector>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <sstream>

#include "settings.h"
#include "word.h"
#include "dictionary.h"
#include "query.h"
#include "nfa.h"
#include "timer.h"


static Dictionary dict;
static std::unordered_map<DictHash, std::vector<int>> prefixMap;
static Nfa<size_t> nfa;

#ifdef PRINT_STATISTICS
    static std::atomic<int> calcCount{0};
    static std::atomic<int> sortCount{0};
    static std::atomic<int> writeStringCount{0};
#endif

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
            ngrams.emplace_back(0);
            dict.createWord(line, 0, ngrams.at(ngrams.size() - 1).hashList);
        }
    }

    return ngrams;
}

void find_in_document(Query& query, const std::vector<Word>& ngrams)
{
    std::vector<Match> matches;
    size_t timestamp = query.timestamp;
    std::string& line = query.document;
    line += ' ';

    NfaVisitor visitor;

#ifdef PRINT_STATISTICS
    Timer timer;
    timer.start();
#endif

    std::string prefix;
    for (size_t i = 2; i < line.size(); i++)
    {
        char c = line.at(i);
        if (c == ' ')
        {
            DictHash hash = dict.get_hash_maybe(prefix);
            if (hash != HASH_NOT_FOUND)
            {
                std::vector<ssize_t> indices;
                nfa.feedWord(visitor, hash, indices);

                for (ssize_t index : indices)
                {
                    const Word& word = ngrams.at(index);
                    if (word.is_active(timestamp))
                    {
                        std::string w = dict.createString(word);

                        matches.emplace_back(i - w.size(), w);  // TODO: subtract string length
                    }
                }
            }
            else visitor.states[visitor.stateIndex].clear();

            prefix.clear();
        }
        else
        {
            prefix += c;
        }
    }

#ifdef PRINT_STATISTICS
    calcCount += timer.get();
    timer.reset();
#endif

    std::sort(matches.begin(), matches.end(), [](Match& m1, Match& m2)
    {
        if (m1.index < m2.index) return true;
        else if (m1.index > m2.index) return false;
        else return m1.word.size() <= m2.word.size();
    });

#ifdef PRINT_STATISTICS
    sortCount += timer.get();
    timer.reset();
#endif

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
        if (found.find(match.word) == found.end())
        {
            query.result += '|';
            query.result += match.word;
            found.insert(match.word);
        }
    }

#ifdef PRINT_STATISTICS
    writeStringCount += timer.get();
#endif
}

int main()
{
    std::ios::sync_with_stdio(false);

    initLinearMap();

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
    size_t query_length = 0, ngram_length = 0, ngram_hashes_length = 0, document_word_count = 0;
    size_t batch_count = 0, batch_size = 0, result_length = 0;
    Timer writeResultTimer;
#endif

    std::vector<Word> ngrams = load_init_data(input);

    for (size_t i = 0; i < ngrams.size(); i++)
    {
        DictHash prefix = ngrams.at(i).hashList.at(0);
        prefixMap[prefix].emplace_back(i);

        nfa.addWord(ngrams.at(i), i);

#ifdef PRINT_STATISTICS
        init_ngrams++;
        ngram_length += dict.createString(ngrams.at(i)).size();
        ngram_hashes_length += ngrams.at(i).hashList.size();
#endif
    }

    std::vector<Query> queries;
    queries.resize(100000);
    size_t timestamp = 0;
    size_t queryIndex = 0;

    std::cout << "R" << std::endl;

    while (true)
    {
        timestamp++;

        std::string& line = queries.at(queryIndex).document;
        if (!std::getline(input, line) || line.size() == 0) break;
        char op = line[0];

        if (op == 'A')
        {
            ngrams.emplace_back(timestamp);
            dict.createWord(line, 2, ngrams.at(ngrams.size() - 1).hashList);
            size_t index = ngrams.size() - 1;
            DictHash prefix = ngrams.at(index).hashList.at(0);
            prefixMap[prefix].emplace_back(index);

            nfa.addWord(ngrams.at(index), index);

#ifdef PRINT_STATISTICS
        additions++;
        ngram_length += line.size();
        ngram_hashes_length += ngrams.at(index).hashList.size();
#endif
        }
        else if (op == 'D')
        {
            Word word(0);
            dict.createWord(line, 2, word.hashList);
            DictHash prefix = word.hashList.at(0);

            auto it = prefixMap.find(prefix);
            if (it != prefixMap.end())
            {
                for (int i : it->second)
                {
                    Word& ngram = ngrams.at(i);
                    if (ngram.is_active(timestamp) && ngram == word)
                    {
                        ngram.deactivate(timestamp);
                    }
                }
            }

#ifdef PRINT_STATISTICS
            deletions++;
#endif
        }
        else if (op == 'Q')
        {
            queries.at(queryIndex).timestamp = timestamp;
            queries.at(queryIndex).result.clear();
            queryIndex++;

            if (queryIndex >= queries.size())
            {
                queries.resize(queries.size() * 2);
            }

#ifdef PRINT_STATISTICS
            query_count++;
            query_length += line.size();

            document_word_count++;
            for (size_t i = 0; i < line.size(); i++)
            {
                if (line.at(i) == ' ')
                {
                    document_word_count++;
                }
            }
#endif
        }
        else
        {
#ifdef PRINT_STATISTICS
            batch_count++;
            batch_size += queries.size();
#endif
            // do queries in parallel
            #pragma omp parallel for schedule(dynamic)
            for (size_t i = 0; i < queryIndex; i++)
            {
                Query& query = queries.at(i);
                find_in_document(query, ngrams);
            }

            // print results
#ifdef PRINT_STATISTICS
            writeResultTimer.start();
#endif
            for (size_t i = 0; i < queryIndex; i++)
            {
#ifdef PRINT_STATISTICS
                result_length += queries.at(i).result.size();
#endif
                std::cout << queries.at(i).result << std::endl;
            }
#ifdef PRINT_STATISTICS
            writeResultTimer.add();
#endif
            queryIndex = 0;

            std::cout << std::flush;
        }
    }

#ifdef PRINT_STATISTICS
    std::cerr << "Initial ngrams: " << init_ngrams << std::endl;
    std::cerr << "Additions: " << additions << std::endl;
    std::cerr << "Deletions: " << deletions << std::endl;
    std::cerr << "Queries: " << query_count << std::endl;
    std::cerr << "Average document length: " << query_length / (double) query_count << std::endl;
    std::cerr << "Average document word count: " << document_word_count / (double) query_count << std::endl;
    std::cerr << "Average ngram length: " << ngram_length / (double) ngrams.size() << std::endl;
    std::cerr << "Average ngram word count: " << ngram_hashes_length / (double) ngrams.size() << std::endl;
    std::cerr << "Average result length: " << result_length / (double) query_count << std::endl;
    std::cerr << "Batch count: " << batch_count << std::endl;
    std::cerr << "Average batch size: " << batch_size / (double) batch_count << std::endl;
    std::cerr << "Calc time: " << calcCount << std::endl;
    std::cerr << "Sort time: " << sortCount << std::endl;
    std::cerr << "Write string time: " << writeStringCount << std::endl;
    std::cerr << "Write result time: " << writeResultTimer.total << std::endl;
#endif

    return 0;
}
