#include <iostream>
#include <vector>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <unistd.h>

#include "settings.h"
#include "word.h"
#include "dictionary.h"
#include "query.h"
#include "nfa.h"
#include "timer.h"
#include <omp.h>

static Dictionary* dict;
static SimpleMap<std::string, DictHash>* wordMap;
static Nfa<size_t>* nfa;
static std::vector<Word>* ngrams;
static std::vector<Query>* queries;

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
    static Timer prehashTimer;
    static Timer computeTimer;
    static double feedTime{0};
    static double addCreateWord{0};
    static double addWordMap{0};
    static double addNfaAdd{0};
    static std::unordered_map<int, int> prefixCounter;
    static std::unordered_map<int, int> endPrefixCounter;

    static int additions = 0, deletions = 0, query_count = 0, init_ngrams = 0;
    static size_t query_length = 0, ngram_length = 0, document_word_count = 0;
    static size_t batch_count = 0, batch_size = 0, result_length = 0;
    static size_t total_word_length = 0;
    static Timer writeResultTimer;

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

std::vector<Word>* load_init_data(std::istream& input)
{
    std::vector<Word>* ngrams = new std::vector<Word>();

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
            ngrams->emplace_back(0, line.size());
            size_t index = ngrams->size() - 1;
            dict->createWordNfa(line, 0, *nfa, index);
            (*wordMap).insert(line, (DictHash)(index));
#ifdef PRINT_STATISTICS
            updateNgramStats(line);
#endif
        }
    }

    return ngrams;
}

void hash_document(Query& query)
{
    /*std::string& line = query.document;

    int size = (int) line.size();
    std::string prefix;
    bool skip = false;
    int i = 2;
    for (; i < size; i++)
    {
        char c = line[i];
        if (c == ' ')
        {
            DictHash hash = dict->map.get(prefix);
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

    query.wordHashes.emplace_back(i, dict->map.get(prefix));*/
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
    Timer feedTimer;
#endif

    std::vector<ssize_t> indices;
    int size = (int) query.document.size();
    std::string& line = query.document;
    std::string prefix;
    int i = 2;
    for (; i < size; i++)
    {
        char c = line[i];
        if (__builtin_expect(c == ' ', false))
        {
            DictHash hash = dict->map.get(prefix);
            if (__builtin_expect(hash != HASH_NOT_FOUND, true))
            {
                nfa->feedWord(visitor, hash, indices);
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
                        matches.emplace_back(i - word.length, word.length);
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

            prefix.clear();
        }
        else prefix += c;
    }

    DictHash hash = dict->map.get(prefix);
    if (hash != HASH_NOT_FOUND)
    {
        nfa->feedWord(visitor, hash, indices);
        for (ssize_t index : indices)
        {
            const Word& word = ngrams[index];
            if (word.is_active(timestamp) && found.find(index) == found.end())
            {
                found.insert(index);
                matches.emplace_back(i - word.length, word.length);
            }
        }
    }

#ifdef PRINT_STATISTICS
    feedTime += feedTimer.total;
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

void delete_ngram(std::string& line, size_t timestamp)
{
    DictHash index = wordMap->get(line.substr(2));
    if (index != HASH_NOT_FOUND)
    {
        Word& ngram = (*ngrams)[index];
        if (ngram.is_active(timestamp))
        {
            ngram.deactivate(timestamp);
        }
    }
}
void add_ngram(const std::string& line, size_t timestamp)
{
#ifdef PRINT_STATISTICS
    Timer addTimer;
#endif
    (*ngrams).emplace_back(timestamp, line.size() - 2);
#ifdef PRINT_STATISTICS
    addTimer.start();
#endif
    size_t index = (*ngrams).size() - 1;
    //dict->createWord(line, 2, (*ngrams)[index].hashList);
#ifdef PRINT_STATISTICS
    addCreateWord += addTimer.get();
    addTimer.start();
#endif
    (*wordMap).insert(line.substr(2), (DictHash) index);
#ifdef PRINT_STATISTICS
    addWordMap += addTimer.get();
    addTimer.start();
#endif
    //nfa->addWord((*ngrams)[index], index);

    dict->createWordNfa<size_t>(line, 2, *nfa, index);
#ifdef PRINT_STATISTICS
    addNfaAdd += addTimer.get();
#endif
}
void query(size_t& queryIndex, size_t timestamp)
{
    (*queries)[queryIndex].reset(timestamp);
    queryIndex++;

    if (queryIndex >= queries->size())
    {
        queries->resize(queries->size() * 2);
    }
}
void batch(size_t& queryIndex)
{
#ifdef PRINT_STATISTICS
    batchTimer.start();
    batch_count++;
    batch_size += queryIndex;
    prehashTimer.start();
#endif
    // hash queries in parallel
    /*#pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < queryIndex; i++)
    {
        hash_document((*queries)[i]);
    }*/

#ifdef PRINT_STATISTICS
    prehashTimer.add();
    computeTimer.start();
#endif
    // do queries in parallel
    #pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < queryIndex; i++)
    {
        find_in_document((*queries)[i], *ngrams);
    }
#ifdef PRINT_STATISTICS
    computeTimer.add();
#endif

    // print results
#ifdef PRINT_STATISTICS
    writeResultTimer.start();
#endif
    for (size_t i = 0; i < queryIndex; i++)
    {
#ifdef PRINT_STATISTICS
        result_length += (*queries)[i].result.size();
#endif
        write(STDOUT_FILENO, (*queries)[i].result.c_str(), (*queries)[i].result.size());
        write(STDOUT_FILENO, "\n", 1);
    }
#ifdef PRINT_STATISTICS
    batchTimer.add();
    writeResultTimer.add();
#endif
    queryIndex = 0;
}

void init(std::istream& input)
{
    dict = new Dictionary();
    wordMap = new SimpleMap<std::string, DictHash>(2000000);
    nfa = new Nfa<size_t>();

    omp_set_num_threads(THREAD_COUNT);
    std::ios::sync_with_stdio(false);

    initLinearMap();

    ngrams = load_init_data(input);

    /*for (size_t i = 0; i < ngrams->size(); i++)
    {
        nfa->addWord((*ngrams)[i], i);

#ifdef PRINT_STATISTICS
        endPrefixCounter[(*ngrams)[i].hashList[(*ngrams)[i].hashList.size() - 1]]++;
        init_ngrams++;
        ngram_length += (*ngrams)[i].length;
#endif
    }*/
}

int main()
{
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

    init(input);

    queries = new std::vector<Query>();
    queries->resize(100000);
    size_t timestamp = 0;
    size_t queryIndex = 0;

    write(STDOUT_FILENO, "R\n", 2);

#ifdef PRINT_STATISTICS
    Timer runTimer;
    runTimer.start();
#endif

    while (true)
    {
        timestamp++;

#ifdef PRINT_STATISTICS
        ioTimer.start();
#endif
        std::string& line = (*queries)[queryIndex].document;
        if (!std::getline(input, line))
        {
#ifdef PRINT_STATISTICS
            ioTimer.add();
#endif
            break;
        }
        char op = line[0];

#ifdef PRINT_STATISTICS
        ioTimer.add();
#endif

        if (op == 'A')
        {
#ifdef PRINT_STATISTICS
            addTimer.start();
#endif
            add_ngram(line, timestamp);

#ifdef PRINT_STATISTICS
            addTimer.add();
            //prefixCounter[prefix]++;
            //endPrefixCounter[(*ngrams)[index].hashList[(*ngrams)[index].hashList.size() - 1]]++;
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
            delete_ngram(line, timestamp);

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
            query(queryIndex, timestamp);

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
            batch(queryIndex);
        }
    }

#ifdef PRINT_STATISTICS
    /*std::vector<std::tuple<int, int>> prefixes;
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
    }*/

    size_t nfaEdgeCount = 0;
    for (auto& state : nfa->states)
    {
        nfaEdgeCount += state.get_size();
    }

    size_t bucketSize = 0;
    for (size_t i = 0; i < dict->map.capacity; i++)
    {
        bucketSize += dict->map.nodes[i].size();
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
    std::cerr << "Average NFA state edge count: " << nfaEdgeCount / (double) (nfa.states.size() - 1) << std::endl;
    std::cerr << "Hash found: " << hashFound << std::endl;
    std::cerr << "Hash not found: " << hashNotFound << std::endl;
    std::cerr << "Inactive ngrams found: " << noActiveFound << std::endl;
    std::cerr << "Duplicate ngrams found: " << duplicateNgramFound << std::endl;
    std::cerr << "NoDuplicate ngrams found: "<< noDuplicateFound << std::endl;
    std::cerr << "Batch count: " << batch_count << std::endl;
    std::cerr << "Average batch size: " << batch_size / (double) batch_count << std::endl;
    std::cerr << "Average SimpleHashMap bucket size: " << bucketSize / (double) dict.map.capacity << std::endl;*/
    std::cerr << "Calc time: " << calcCount << std::endl;
    std::cerr << "Sort time: " << sortCount << std::endl;
    std::cerr << "String recreate time: " << stringCreateTime << std::endl;
    std::cerr << "Create result time: " << writeStringCount << std::endl;
    std::cerr << "Write result time: " << writeResultTimer.total << std::endl;
    std::cerr << "IO time: " << ioTimer.total << std::endl;
    std::cerr << "Add time: " << addTimer.total << std::endl;
    std::cerr << "Add create word time: " << addCreateWord << std::endl;
    std::cerr << "Add word map time: " << addWordMap << std::endl;
    std::cerr << "Add nfa add time: " << addNfaAdd << std::endl;
    std::cerr << "Delete time: " << deleteTimer.total << std::endl;
    std::cerr << "Query time: " << queryTimer.total << std::endl;
    std::cerr << "Batch time: " << batchTimer.total << std::endl;
    std::cerr << "Prehash time: " << prehashTimer.total << std::endl;
    std::cerr << "Find time: " << computeTimer.total << std::endl;
    std::cerr << "Feed time: " << feedTime << std::endl;
    std::cerr << "Run time: " << runTimer.total << std::endl;
    std::cerr << std::endl;
#endif

    return 0;
}
