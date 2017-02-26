#include <iostream>
#include <vector>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <omp.h>
#include <unistd.h>

#include "settings.h"
#include "word.h"
#include "dictionary.h"
#include "query.h"
#include "timer.h"

static Dictionary* dict;
static SimpleMap<std::string, DictHash>* wordMap;
static Nfa<size_t>* nfa;
static std::vector<Query>* queries;

#ifdef PRINT_STATISTICS
    static std::atomic<int> calcCount{0};
    static std::atomic<int> sortCount{0};
    static std::atomic<int> writeStringCount{0};
    static Timer addTimer;
    static Timer deleteTimer;
    static Timer queryTimer;
    static Timer batchTimer;
    static Timer splitJobsTimer;
    static Timer computeTimer;
    static Timer writeResultTimer;
    static Timer ioTimer;
    static double feedTime{0};
    static double addCreateWord{0};

    static int additions = 0, deletions = 0, query_count = 0;
    static size_t query_length = 0, ngram_length = 0, document_word_count = 0;
    static size_t batch_count = 0, batch_size = 0;
    static size_t total_word_length = 0;
#endif

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
            size_t prefixHash;
            size_t index = dict->createWordNfa(line, 0, *nfa, 0, prefixHash);
            (*wordMap).insert("A " + line, (DictHash)(index));
        }
    }
}

bool ends_with(std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

void loadWord(int from, int length, std::string& target, const std::string& source)
{
    int to = from + length;
    for (; from < to; from++)
    {
        target += source.at(from);
    }
}
void find_in_document(Query& query)
{
    std::vector<Match> matches;
    std::unordered_set<unsigned int> found;
    size_t timestamp = query.timestamp;

    NfaVisitor visitor;

#ifdef PRINT_STATISTICS
    Timer timer;
    timer.start();
    Timer feedTimer;
#endif

    std::vector<std::pair<unsigned int, unsigned int>> indices; // id, length
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
                nfa->feedWord(visitor, hash, indices, timestamp, true);
                for (auto wordInfo : indices)
                {
#ifdef PRINT_STATISTICS
#endif
                    if (found.find(wordInfo.first) == found.end())
                    {
                        found.insert(wordInfo.first);
#ifdef PRINT_STATISTICS
                        Timer stringTimer;
                        stringTimer.start();
#endif
                        matches.emplace_back(i - wordInfo.second, wordInfo.second);
                    }
                }
                indices.clear();
            }
            else
            {
                visitor.states[visitor.stateIndex].clear();
            }

            prefix.clear();
        }
        else prefix += c;
    }

    DictHash hash = dict->map.get(prefix);
    if (hash != HASH_NOT_FOUND)
    {
        nfa->feedWord(visitor, hash, indices, timestamp, true);
        for (auto wordInfo : indices)
        {
            if (found.find(wordInfo.first) == found.end())
            {
                matches.emplace_back(i - wordInfo.second, wordInfo.second);
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
void find_in_document(Query& query, int from, int to, std::vector<std::string>& result)
{
    std::vector<Match> matches;
    std::unordered_set<unsigned int> found;
    size_t timestamp = query.timestamp;

    NfaVisitor visitor;

#ifdef PRINT_STATISTICS
    Timer timer;
    timer.start();
    Timer feedTimer;
#endif

    std::vector<std::pair<unsigned int, unsigned int>> indices; // id, length
    int size = (int) query.document.size();
    std::string& line = query.document;
    std::string prefix;
    size_t prefixHash;
    HASH_INIT(prefixHash);

    if (line[from - 1] != ' ' && line[from] != ' ')
    {
        while (from < size && line[from] != ' ') from++;
        if (from >= size) return;
    }
    if (line[from] == ' ') from++;

    bool checkAfter = true;
    bool startNewWords = true;

    int i = from;
    for (; i < size; i++)
    {
        char c = line[i];
        if (__builtin_expect(c == ' ', false))
        {
            DictHash hash = dict->map.get_hash(prefix, prefixHash);
            if (__builtin_expect(hash != HASH_NOT_FOUND, true))
            {
                nfa->feedWord(visitor, hash, indices, timestamp, startNewWords);
                for (auto wordInfo : indices)
                {
                    if (found.find(wordInfo.first) == found.end())
                    {
                        found.insert(wordInfo.first);

                        matches.emplace_back(i - wordInfo.second, wordInfo.second);
                    }
                }
                indices.clear();
            }
            else visitor.states[visitor.stateIndex].clear();

            if (i >= to && visitor.states[visitor.stateIndex].empty())
            {
                checkAfter = false;
                break;
            }

            HASH_INIT(prefixHash);
            prefix.clear();

            startNewWords = i < to;
        }
        else
        {
            prefix += c;
            HASH_UPDATE(prefixHash, c);
        }
    }

    if (checkAfter)
    {
        DictHash hash = dict->map.get_hash(prefix, prefixHash);
        if (hash != HASH_NOT_FOUND)
        {
            nfa->feedWord(visitor, hash, indices, timestamp, startNewWords);
            for (auto wordInfo : indices)
            {
                if (found.find(wordInfo.first) == found.end())
                {
                    matches.emplace_back(i - wordInfo.second, wordInfo.second);
                }
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

    for (size_t i = 0; i < matches.size(); i++)
    {
        Match& match = matches[i];
        result.emplace_back();
        loadWord(match.index, match.length, result[result.size() - 1], query.document);
    }

#ifdef PRINT_STATISTICS
    writeStringCount += timer.get();
#endif
}

void delete_ngram(std::string& line, size_t timestamp)
{
    line[0] = 'A';
    DictHash index = wordMap->get(line);
    if (index != HASH_NOT_FOUND)
    {
        Word& ngram = nfa->states[index].word;
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
    size_t wordHash;
    HASH_INIT(wordHash);
    HASH_UPDATE(wordHash, 'A');
    HASH_UPDATE(wordHash, ' ');

    size_t index = dict->createWordNfa<size_t>(line, 2, *nfa, timestamp, wordHash);
#ifdef PRINT_STATISTICS
    addCreateWord += addTimer.get();
#endif
    (*wordMap).insert_hash(line, (DictHash) index, wordHash);
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

static std::string ioResult;
void batch_split(size_t queryIndex)
{
#ifdef PRINT_STATISTICS
    splitJobsTimer.start();
#endif
    if (queryIndex < 1) return;

    size_t total_size = 0;
    for (size_t i = 0; i < queryIndex; i++)
    {
        total_size += (*queries)[i].document.size() - 2;
    }

    size_t workerPart = static_cast<size_t>(std::ceil(total_size / (double) THREAD_COUNT));
    size_t workerLeft = workerPart;
    std::vector<QueryRegion> regions[THREAD_COUNT];
    size_t threadId = 0;
    size_t queryId = 0;
    size_t from = 2;
    size_t sum = 0;

    while (threadId < THREAD_COUNT && queryId < queryIndex)
    {
        size_t leftInQuery = (*queries)[queryId].document.size() - from;
        if (leftInQuery == 0)
        {
            from = 2;
            queryId++;
        }
        else if (workerLeft == 0)
        {
            threadId++;
            workerLeft = workerPart;
        }
        else
        {
            size_t leftInQueryWorker = std::min(leftInQuery, workerLeft);
            regions[threadId].emplace_back(queryId, from, from + leftInQueryWorker);
            sum += leftInQueryWorker;
            from += leftInQueryWorker;
            workerLeft -= leftInQueryWorker;
        }
    }

#ifdef PRINT_STATISTICS
    splitJobsTimer.add();
    computeTimer.start();
#endif

    // do queries in parallel
    #pragma omp parallel
    //for (int tid = 0; tid < THREAD_COUNT; tid++)
    {
        int tid = omp_get_thread_num();
        for (size_t i = 0; i < regions[tid].size(); i++)
        {
            QueryRegion& region = regions[tid][i];
            find_in_document((*queries)[region.queryIndex], region.from, region.to, region.result);
        }
    }

    // print results
#ifdef PRINT_STATISTICS
    computeTimer.add();
    writeResultTimer.start();
#endif

    int lastQueryId = 0;
    bool queryFirst = true;
    bool queryOutputted = false;
    std::unordered_set<std::string> found;
    for (size_t tid = 0; tid < THREAD_COUNT; tid++)
    {
        for (size_t i = 0; i < regions[tid].size(); i++)
        {
            QueryRegion& region = regions[tid][i];
            if (region.queryIndex != lastQueryId)
            {
                if (!queryOutputted)
                {
                    ioResult += "-1\n";
                }
                else ioResult += '\n';

                lastQueryId = region.queryIndex;
                queryOutputted = false;
                queryFirst = true;
                found.clear();
            }

            if (region.result.size() > 0)
            {
                bool firstFound = found.find(region.result[0]) == found.end();

                if (queryFirst)
                {
                    queryFirst = false;
                }
                else if (firstFound)
                {
                    ioResult += '|';
                }

                if (firstFound)
                {
                    ioResult += region.result[0];
                    found.insert(region.result[0]);
                }

                for (size_t str = 1; str < region.result.size(); str++)
                {
                    if (found.find(region.result[str]) == found.end())
                    {
                        found.insert(region.result[str]);
                        ioResult += '|';
                        ioResult += region.result[str];
                    }
                }

                queryOutputted = true;
            }
        }
    }

    if (!queryOutputted)
    {
        ioResult += "-1\n";
    }
    else ioResult += '\n';

    write(STDOUT_FILENO, ioResult.c_str(), ioResult.size());
    ioResult.clear();

#ifdef PRINT_STATISTICS
    writeResultTimer.add();
#endif
}
void batch(size_t& queryIndex)
{
#ifdef PRINT_STATISTICS
    batchTimer.start();
    batch_count++;
    batch_size += queryIndex;
#endif
    if (queryIndex <= THREAD_COUNT)
    {
        batch_split(queryIndex);
    }
    else
    {
        #pragma omp parallel for schedule(dynamic)
        for (size_t i = 0; i < queryIndex; i++)
        {
            find_in_document((*queries)[i]);
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
            write(STDOUT_FILENO, (*queries)[i].result.c_str(), (*queries)[i].result.size());
            write(STDOUT_FILENO, "\n", 1);
        }
#ifdef PRINT_STATISTICS
        writeResultTimer.add();
#endif
    }


#ifdef PRINT_STATISTICS
    batchTimer.add();
#endif
    queryIndex = 0;
}

void init(std::istream& input)
{
    dict = new Dictionary();
    wordMap = new SimpleMap<std::string, DictHash>(2 << 20);
    nfa = new Nfa<size_t>();
    ioResult.reserve(100000);

    omp_set_num_threads(THREAD_COUNT);
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    initLinearMap();

    load_init_data(input);
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
            additions++;
            ngram_length += line.size();
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
        else batch(queryIndex);
    }

#ifdef PRINT_STATISTICS
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

    /*std::cerr << "Additions: " << additions << std::endl;
    std::cerr << "Deletions: " << deletions << std::endl;
    std::cerr << "Queries: " << query_count << std::endl;
    std::cerr << "Average document length: " << query_length / (double) query_count << std::endl;
    std::cerr << "Average document word count: " << document_word_count / (double) query_count << std::endl;
    std::cerr << "Average document word length: " << total_word_length / document_word_count << std::endl;
    std::cerr << "Average result length: " << result_length / (double) query_count << std::endl;
    std::cerr << "Batch count: " << batch_count << std::endl;
    std::cerr << "Average batch size: " << batch_size / (double) batch_count << std::endl;
    std::cerr << "Average SimpleHashMap bucket size: " << bucketSize / (double) dict->map.capacity << std::endl;
    std::cerr << "Calc time: " << calcCount << std::endl;
    std::cerr << "Sort time: " << sortCount << std::endl;
    std::cerr << "Create result time: " << writeStringCount << std::endl;
    std::cerr << "IO time: " << ioTimer.total << std::endl;
    std::cerr << "Add time: " << addTimer.total << std::endl;
    std::cerr << "Add create word time: " << addCreateWord << std::endl;
    std::cerr << "Delete time: " << deleteTimer.total << std::endl;
    std::cerr << "Query time: " << queryTimer.total << std::endl;*/
    std::cerr << "Batch time: " << batchTimer.total << std::endl;
    std::cerr << "Split jobs time: " << splitJobsTimer.total << std::endl;
    std::cerr << "Find time: " << computeTimer.total << std::endl;
    std::cerr << "Write result time: " << writeResultTimer.total << std::endl;
    /*std::cerr << "Feed time: " << feedTime << std::endl;
    std::cerr << "Run time: " << runTimer.total << std::endl;*/
    std::cerr << std::endl;
#endif

    return 0;
}
