#include <iostream>
#include <vector>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <omp.h>

#include "settings.h"
#include "word.h"
#include "dictionary.h"
#include "query.h"


static Dictionary dict;
static std::unordered_map<DictHash, std::vector<int>> prefixMap;

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
            ngrams.emplace_back(dict.createWord(line), 0);
        }
    }

    return ngrams;
}

void find_in_document(Query& query, const std::vector<Word>& ngrams)
{
    std::vector<Match> matches;
    size_t timestamp = query.timestamp;
    std::string& line = query.document;

    Word lineWord(dict.createWordNoInsert(line), 0);

    for (size_t i = 0; i < lineWord.hashList.size(); i++)
    {
        DictHash prefix = lineWord.hashList.at(i);
        if (prefix != HASH_NOT_FOUND && prefixMap.count(prefix))
        {
            for (int index : prefixMap.at(prefix))
            {
                const Word& ngram = ngrams.at(index);

                if (!ngram.is_active(timestamp)) continue;

                size_t ngramSize = ngram.hashList.size();
                size_t lineSize = lineWord.hashList.size();
                size_t matchedLength = 0;
                for (; matchedLength < ngramSize && i + matchedLength < lineSize; matchedLength++)
                {
                    if (lineWord.hashList.at(i) == HASH_NOT_FOUND || ngram.hashList.at(matchedLength) != lineWord.hashList.at(i + matchedLength))
                    {
                        break;
                    }
                }

                if (matchedLength == ngramSize)
                {
                    matches.emplace_back(i, dict.createString(ngram));
                }
            }
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
    size_t query_length = 0, ngram_length = 0, document_word_count = 0;
    size_t batch_count = 0, batch_size = 0;
#endif

    std::vector<Word> ngrams = load_init_data(input);

    for (size_t i = 0; i < ngrams.size(); i++)
    {
        DictHash prefix = ngrams.at(i).hashList.at(0);
        if (!prefixMap.count(prefix))
        {
            prefixMap[prefix] = std::vector<int>();
        }
        prefixMap.at(prefix).emplace_back(i);

#ifdef PRINT_STATISTICS
        init_ngrams++;
        ngram_length += dict.createString(ngrams.at(i)).size();
#endif
    }

    std::cout << "R" << std::endl;

    std::vector<Query> queries;
    size_t timestamp = 0;

    while (true)
    {
        std::string line;
        if (!std::getline(input, line) || line.size() == 0) break;
        char op = line[0];
        timestamp++;

        if (op == 'A')
        {
            line = line.substr(2);

            ngrams.emplace_back(dict.createWord(line), timestamp);
            size_t index = ngrams.size() - 1;
            DictHash prefix = ngrams.at(index).hashList.at(0);
            if (!prefixMap.count(prefix))
            {
                prefixMap[prefix] = std::vector<int>();
            }
            prefixMap.at(prefix).emplace_back(index);

#ifdef PRINT_STATISTICS
        additions++;
        ngram_length += line.size();
#endif
        }
        else if (op == 'D')
        {
            line = line.substr(2);

            Word word(dict.createWord(line), 0);
            DictHash prefix = word.hashList.at(0);

            if (prefixMap.count(prefix))
            {
                for (int i : prefixMap.at(prefix))
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
            line = line.substr(2);

            queries.emplace_back(line, timestamp);

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
            for (size_t i = 0; i < queries.size(); i++)
            {
                Query& query = queries.at(i);
                find_in_document(query, ngrams);
            }

            // print results
            for (Query& query : queries)
            {
                std::cout << query.result << std::endl;
            }

            queries.clear();

            std::cout << std::flush;
        }
    }

#ifdef PRINT_STATISTICS
    std::cerr << "Initial ngrams: " << init_ngrams << std::endl;
    std::cerr << "Additions: " << additions << std::endl;
    std::cerr << "Deletions: " << deletions << std::endl;
    std::cerr << "Queries: " << query_count << std::endl;
    std::cerr << "Average document length: " << query_length / query_count << std::endl;
    std::cerr << "Average document word count: " << document_word_count / query_count << std::endl;
    std::cerr << "Average ngram length: " << ngram_length / ngrams.size() << std::endl;
    std::cerr << "Batch count: " << batch_count << std::endl;
    std::cerr << "Average batch size: " << batch_size / batch_count << std::endl;

#endif

    return 0;
}
