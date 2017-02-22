#include <iostream>
#include <vector>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <atomic>

#include "settings.h"
#include "word.h"
#include "dictionary.h"
#include "query.h"
#include "nfa.h"
#include "timer.h"
#include <omp.h>

static Dictionary dict;
static std::unordered_map<DictHash, std::vector<int>> prefixMap;
static Nfa<size_t> nfa;

#ifdef PRINT_STATISTICS
    static size_t ngram_word_count = 0;
    static size_t ngram_word_length = 0;
    static std::atomic<int> calcCount{0};
    static std::atomic<int> sortCount{0};
    static std::atomic<int> writeStringCount{0};
    static std::atomic<int> hashFound{0};
    static std::atomic<int> hashNotFound{0};
    static std::atomic<int> stringCreateTime{0};
    static std::atomic<int> noActiveFound{0};
    static std::atomic<int> duplicateNgramFound{0};
    static std::atomic<int> noDuplicateFound{0};
    static std::atomic<int> nfaIterationCount{0};
    static std::atomic<int> nfaStateCount{0};
    static Timer addTimer;
    static Timer deleteTimer;
    static Timer queryTimer;
    static Timer batchTimer;
    static Timer ioTimer;
    static Timer addCharCacheTimer;
    static std::atomic<int> readCharCache{0};
    static std::atomic<int> charCacheHit{0};
    static std::atomic<int> charCacheMiss{0};
    static std::unordered_map<int, int> prefixCounter;
    static std::unordered_map<int, int> endPrefixCounter;

void updateNgramStats(const std::string& line)
{
    std::string prefix;
    for (size_t i = 0; i < line.size(); i++)
    {
        if (line[i] == ' ')
        {
            ngram_word_length += prefix.size();
            ngram_word_count++;
            prefix.clear();
        }
        else prefix += line[i];
    }

    ngram_word_count++;
    ngram_word_length += prefix.size();
}
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
            ngrams.emplace_back(0, line.size());
            dict.createWord(line, 0, ngrams[ngrams.size() - 1].hashList);
#ifdef PRINT_STATISTICS
            updateNgramStats(line);
#endif
        }
    }

    return ngrams;
}

void hash_document(Query& query)
{
    std::string& line = query.document;

    int size = (int) line.size();
    std::string prefix;
    bool skip = false;
    int i = 2;
    for (; i < size; i++)
    {
        char c = line[i];
        if (c == ' ')
        {
            DictHash hash = dict.map.get(prefix);
            if (hash != HASH_NOT_FOUND)
            {
                query.wordHashes.emplace_back(i, hash);
                skip = false;
            }
            else if (!skip)
            {
                skip = true;
                query.wordHashes.emplace_back(i, HASH_NOT_FOUND);
            }
            prefix.clear();
        }
        else prefix += c;
    }

    query.wordHashes.emplace_back(i, dict.map.get(prefix));
}

void loadWord(int from, int length, std::string& target, const std::string& source)
{
    /*int to = from + length;
    for (; from < to; from++)
    {
        target += source.at(from);
    }*/
    target += source.substr(from, length);
}
void find_in_document(Query& query, const std::vector<Word>& ngrams)
{
    std::vector<Match> matches;
    std::unordered_set<ssize_t> found;
    size_t timestamp = query.timestamp;

    NfaVisitor visitor;

#ifdef PRINT_STATISTICS
    Timer timer;
    timer.start();
    Timer readCharCacheTimer;
#endif

    std::vector<ssize_t> indices;
    int size = (int) query.wordHashes.size();
    for (int i = 0; i < size; i++)
    {
        DictHash hash = query.wordHashes[i].hash;
        if (__builtin_expect(hash != HASH_NOT_FOUND, true))    // TODO: check
        {
            nfa.feedWord(visitor, hash, indices);
#ifdef PRINT_STATISTICS
            nfaIterationCount++;
            nfaStateCount += visitor.states[visitor.stateIndex].size();
#endif
            for (ssize_t index : indices)
            {
                const Word& word = ngrams[index];
#ifdef PRINT_STATISTICS
                if (!word.is_active(timestamp))
                {
                    noActiveFound++;
                }
                if (found.find(index) != found.end())
                {
                    duplicateNgramFound++;
                }
                else noDuplicateFound++;
#endif
                if (word.is_active(timestamp) && found.find(index) == found.end())
                {
                    found.insert(index);
#ifdef PRINT_STATISTICS
                    Timer stringTimer;
                    stringTimer.start();
#endif
                    matches.emplace_back(query.wordHashes[i].index - word.length, word.length);
#ifdef PRINT_STATISTICS
                    stringCreateTime += stringTimer.get();
#endif
                }
            }
            indices.clear();
#ifdef PRINT_STATISTICS
            hashFound++;
#endif
        }
        else
        {
            visitor.states[visitor.stateIndex].clear();
#ifdef PRINT_STATISTICS
            hashNotFound++;
#endif
        }
    }

#ifdef PRINT_STATISTICS
    readCharCache += readCharCacheTimer.total;
    calcCount += timer.get();
    timer.reset();
#endif

    std::sort(matches.begin(), matches.end(), [](Match& m1, Match& m2)
    {
        if (m1.index < m2.index) return true;
        else if (m1.index > m2.index) return false;
        else return m1.length <= m2.length;
    });

#ifdef PRINT_STATISTICS
    sortCount += timer.get();
    timer.reset();
#endif

    if (matches.size() == 0)
    {
        query.result += "-1";
    }
    else loadWord(matches[0].index, matches[0].length, query.result, query.document);

    for (size_t i = 1; i < matches.size(); i++)
    {
        Match& match = matches[i];
        query.result += '|';
        loadWord(match.index, match.length, query.result, query.document);
    }

#ifdef PRINT_STATISTICS
    writeStringCount += timer.get();
#endif
}

int main()
{
    omp_set_num_threads(THREAD_COUNT);
    std::ios::sync_with_stdio(false);

    initLinearMap();

#ifdef LOAD_FROM_FILE
    std::fstream file(LOAD_FROM_FILE, std::iostream::in);

    if (!file.is_open())
    {
        //throw "File not found";
    }

    std::istream& input = file;
#else
    std::istream& input = std::cin;
#endif

#ifdef PRINT_STATISTICS
    int additions = 0, deletions = 0, query_count = 0, init_ngrams = 0;
    size_t query_length = 0, ngram_length = 0, document_word_count = 0;
    size_t batch_count = 0, batch_size = 0, result_length = 0;
    size_t total_word_length = 0;
    Timer writeResultTimer;
#endif

    std::vector<Word> ngrams = load_init_data(input);

    for (size_t i = 0; i < ngrams.size(); i++)
    {
        DictHash prefix = ngrams[i].hashList[0];
        prefixMap[prefix].emplace_back(i);

        nfa.addWord(ngrams[i], i);

#ifdef PRINT_STATISTICS
        prefixCounter[prefix]++;
        endPrefixCounter[ngrams[i].hashList[ngrams[i].hashList.size() - 1]]++;
        init_ngrams++;
        ngram_length += ngrams[i].length;
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

#ifdef PRINT_STATISTICS
        ioTimer.start();
#endif
        std::string& line = queries[queryIndex].document;
        if (!std::getline(input, line) || line.size() == 0) break;
        char op = line[0];

#ifdef PRINT_STATISTICS
        ioTimer.add();
#endif

        if (op == 'A')
        {
#ifdef PRINT_STATISTICS
            addTimer.start();
#endif
            ngrams.emplace_back(timestamp, line.size() - 2);
            dict.createWord(line, 2, ngrams[ngrams.size() - 1].hashList);

#ifdef USE_CHAR_CACHE
    #ifdef PRINT_STATISTICS
                addCharCacheTimer.start();
    #endif
                addToCharCache(line, 2);
    #ifdef PRINT_STATISTICS
                addCharCacheTimer.add();
    #endif
#endif
            size_t index = ngrams.size() - 1;
            DictHash prefix = ngrams[index].hashList[0];
            prefixMap[prefix].emplace_back(index);

            nfa.addWord(ngrams[index], index);

#ifdef PRINT_STATISTICS
            prefixCounter[prefix]++;
            endPrefixCounter[ngrams[index].hashList[ngrams[index].hashList.size() - 1]]++;
            addTimer.add();
            additions++;
            ngram_length += line.size();
            updateNgramStats(line.substr(2));
#endif
        }
        else if (op == 'D')
        {
#ifdef PRINT_STATISTICS
            deleteTimer.start();
#endif
            Word word(0, (int) line.size() - 2);
            dict.createWord(line, 2, word.hashList);
            DictHash prefix = word.hashList[0];

            auto it = prefixMap.find(prefix);
            if (it != prefixMap.end())
            {
                for (int i : it->second)
                {
                    Word& ngram = ngrams[i];
                    if (ngram.is_active(timestamp) && ngram == word)
                    {
                        ngram.deactivate(timestamp);
                    }
                }
            }

#ifdef PRINT_STATISTICS
            deleteTimer.add();
            deletions++;
#endif
        }
        else if (op == 'Q')
        {
#ifdef PRINT_STATISTICS
            queryTimer.start();
#endif
            queries[queryIndex].reset(timestamp);
            queryIndex++;

            if (queryIndex >= queries.size())
            {
                queries.resize(queries.size() * 2);
            }

#ifdef PRINT_STATISTICS
            queryTimer.add();
            query_count++;
            query_length += line.size();

            document_word_count++;
            for (size_t i = 2; i < line.size(); i++)
            {
                if (line[i] == ' ')
                {
                    document_word_count++;
                }
                else total_word_length++;
            }
#endif
        }
        else
        {
#ifdef PRINT_STATISTICS
            batchTimer.start();
            batch_count++;
            batch_size += queryIndex;
#endif
            // hash queries in parallel
            #pragma omp parallel for schedule(dynamic)
            for (size_t i = 0; i < queryIndex; i++)
            {
                hash_document(queries[i]);
            }

            // do queries in parallel
            #pragma omp parallel for schedule(dynamic)
            for (size_t i = 0; i < queryIndex; i++)
            {
                find_in_document(queries[i], ngrams);
            }

            // print results
#ifdef PRINT_STATISTICS
            writeResultTimer.start();
#endif
            for (size_t i = 0; i < queryIndex; i++)
            {
#ifdef PRINT_STATISTICS
                result_length += queries[i].result.size();
#endif
                std::cout << queries[i].result << std::endl;
            }
#ifdef PRINT_STATISTICS
            batchTimer.add();
            writeResultTimer.add();
#endif
            queryIndex = 0;

            std::cout << std::flush;
        }
    }

#ifdef PRINT_STATISTICS
    std::vector<std::tuple<int, int>> prefixes;
    size_t prefix_count = 0;
    for (auto& kv : endPrefixCounter)
    {
        prefixes.push_back(std::tuple<int, int>(kv.first, kv.second));
        prefix_count += kv.second;
    }

    std::sort(prefixes.begin(), prefixes.end(), [](std::tuple<int, int>& p1, std::tuple<int, int>& p2) {
        return std::get<1>(p1) >= std::get<1>(p2);
    });

    for (int i = 0; i < 8; i++)
    {
        std::cerr << "Prefix: " << std::get<0>(prefixes[i]) << ", count: " << std::get<1>(prefixes[i]) << std::endl;
    }

    size_t nfaEdgeCount = 0;
    for (auto& state : nfa.states)
    {
        nfaEdgeCount += state.get_size();
    }

    size_t bucketSize = 0;
    for (size_t i = 0; i < dict.map.capacity; i++)
    {
        bucketSize += dict.map.nodes[i].size();
    }

    /*std::cerr << "Initial ngrams: " << init_ngrams << std::endl;
    std::cerr << "Additions: " << additions << std::endl;
    std::cerr << "Deletions: " << deletions << std::endl;
    std::cerr << "Queries: " << query_count << std::endl;
    std::cerr << "Average document length: " << query_length / (double) query_count << std::endl;
    std::cerr << "Average document word count: " << document_word_count / (double) query_count << std::endl;
    std::cerr << "Average document word length: " << total_word_length / document_word_count << std::endl;
    std::cerr << "Average ngram length: " << ngram_length / (double) ngrams.size() << std::endl;
    std::cerr << "Average ngram word count: " << ngram_word_count / (double) ngrams.size() << std::endl;
    std::cerr << "Average ngram word length: " << ngram_word_length / ngram_word_count << std::endl;
    std::cerr << "Average result length: " << result_length / (double) query_count << std::endl;
    std::cerr << "Average prefix ngram count: " << prefix_count / prefixCounter.size() << std::endl;
    std::cerr << "Average NFA visitor state count: " << nfaStateCount / (double) nfaIterationCount << std::endl;
    std::cerr << "NFA root state edge count: " << nfa.rootState.get_size() << std::endl;
    std::cerr << "Average NFA state edge count: " << nfaEdgeCount / (double) (nfa.states.size() - 1) << std::endl;*/
    std::cerr << "Hash found: " << hashFound << std::endl;
    std::cerr << "Hash not found: " << hashNotFound << std::endl;
    std::cerr << "Inactive ngrams found: " << noActiveFound << std::endl;
    std::cerr << "Duplicate ngrams found: " << duplicateNgramFound << std::endl;
    std::cerr << "NoDuplicate ngrams found: "<< noDuplicateFound << std::endl;
    std::cerr << "Batch count: " << batch_count << std::endl;
    std::cerr << "Average batch size: " << batch_size / (double) batch_count << std::endl;
    std::cerr << "Average SimpleHashMap bucket size: " << bucketSize / (double) dict.map.capacity << std::endl;
    std::cerr << "Calc time: " << calcCount << std::endl;
    std::cerr << "Sort time: " << sortCount << std::endl;
    std::cerr << "String recreate time: " << stringCreateTime << std::endl;
    std::cerr << "Create result time: " << writeStringCount << std::endl;
    std::cerr << "Write result time: " << writeResultTimer.total << std::endl;
    std::cerr << "IO time: " << ioTimer.total << std::endl;
    std::cerr << "Add time: " << addTimer.total << std::endl;
    std::cerr << "Delete time: " << deleteTimer.total << std::endl;
    std::cerr << "Query time: " << queryTimer.total << std::endl;
    std::cerr << "Batch time: " << batchTimer.total << std::endl;
    std::cerr << std::endl;
#endif

    return 0;
}
