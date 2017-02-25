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
#include "hash.h"

static Dictionary* dict;
static SimpleMap<std::string, DictHash>* wordMap;
static Nfa<size_t>* nfa;
static std::vector<Query>* queries;

#ifdef PRINT_STATISTICS
    static std::atomic<int> calcCount{0};
    static std::atomic<int> sortCount{0};
    static std::atomic<int> writeStringCount{0};
    static std::atomic<int> foundSetTime{0};
    static Timer addTimer;
    static Timer deleteTimer;
    static Timer queryTimer;
    static Timer batchTimer;
    static Timer ioTimer;
    static Timer prehashTimer;
    static Timer computeTimer;
    static double feedTime{0};
    static double addCreateWord{0};

    static int additions = 0, deletions = 0, query_count = 0;
    static size_t query_length = 0, ngram_length = 0, document_word_count = 0;
    static size_t batch_count = 0, batch_size = 0, result_length = 0;
    static size_t total_word_length = 0;
    static Timer writeResultTimer;
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
    size_t prefixHash;
    HASH_INIT(prefixHash);
    int i = 2;

    for (; i < size; i++)
    {
        char c = line[i];
        if (__builtin_expect(c == ' ', false))
        {
            DictHash hash = dict->map.get_hash(prefix, prefixHash);
            if (__builtin_expect(hash != HASH_NOT_FOUND, true))
            {
                nfa->feedWord(visitor, hash, indices, timestamp);
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
            else
            {
                visitor.states[visitor.stateIndex].clear();
            }

            HASH_INIT(prefixHash);
            prefix.clear();
        }
        else
        {
            prefix += c;
            HASH_UPDATE(prefixHash, c);
        }
    }

    DictHash hash = dict->map.get_hash(prefix, prefixHash);
    if (hash != HASH_NOT_FOUND)
    {
        nfa->feedWord(visitor, hash, indices, timestamp);
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
void batch(size_t& queryIndex)
{
#ifdef PRINT_STATISTICS
    batchTimer.start();
    batch_count++;
    batch_size += queryIndex;
#endif

#ifdef PRINT_STATISTICS
    computeTimer.start();
#endif
    // do queries in parallel
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

/*#define IO_BUFFER_SIZE (1024 * 1024 * 16)
static char* ioBuffer;
static size_t ioStart = 0;
static size_t ioIndex = 0;
bool read_line(std::string& line)
{
    if (feof(stdin)) return false;
    size_t sizeLeft = IO_BUFFER_SIZE - ioIndex;
    ssize_t count = read(STDIN_FILENO, ioBuffer + ioIndex, sizeLeft);
    if (count <= 0) return false;
    ioIndex += count;

    size_t i = ioStart;
    while (i < ioIndex)
    {
        if (ioBuffer[i] == '\n')
        {
            line.resize(i - ioStart);
            std::memcpy(const_cast<char*>(line.c_str()), ioBuffer + ioStart, line.size());
            break;
        }
        i++;
    }

    ioStart = i + 1;

    ioBuffer = new char[IO_BUFFER_SIZE];
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    return true;
}
void clear_buffer()
{
    ioStart = 0;
    ioIndex = 0;
}*/

void init(std::istream& input)
{
    dict = new Dictionary();
    wordMap = new SimpleMap<std::string, DictHash>(2 << 20);
    nfa = new Nfa<size_t>();

    omp_set_num_threads(THREAD_COUNT);
    std::ios::sync_with_stdio(false);

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

    /*std::cerr << "Additions: " << additions << std::endl;
    std::cerr << "Deletions: " << deletions << std::endl;
    std::cerr << "Queries: " << query_count << std::endl;
    std::cerr << "Average document length: " << query_length / (double) query_count << std::endl;
    std::cerr << "Average document word count: " << document_word_count / (double) query_count << std::endl;
    std::cerr << "Average document word length: " << total_word_length / document_word_count << std::endl;
    std::cerr << "Average ngram word length: " << ngram_word_length / ngram_word_count << std::endl;
    std::cerr << "Average result length: " << result_length / (double) query_count << std::endl;
    std::cerr << "Batch count: " << batch_count << std::endl;
    std::cerr << "Average batch size: " << batch_size / (double) batch_count << std::endl;
    std::cerr << "Average SimpleHashMap bucket size: " << bucketSize / (double) dict->map.capacity << std::endl;
    std::cerr << "Indices avg size: " << indicesSize / (double) indicesCount << std::endl;*/
    std::cerr << "Calc time: " << calcCount << std::endl;
    std::cerr << "Sort time: " << sortCount << std::endl;
    std::cerr << "Found set time: " << foundSetTime << std::endl;
    std::cerr << "Create result time: " << writeStringCount << std::endl;
    std::cerr << "Write result time: " << writeResultTimer.total << std::endl;
    std::cerr << "IO time: " << ioTimer.total << std::endl;
    std::cerr << "Add time: " << addTimer.total << std::endl;
    std::cerr << "Add create word time: " << addCreateWord << std::endl;
    std::cerr << "Delete time: " << deleteTimer.total << std::endl;
    std::cerr << "Query time: " << queryTimer.total << std::endl;
    std::cerr << "Batch time: " << batchTimer.total << std::endl;
    std::cerr << "Find time: " << computeTimer.total << std::endl;
    std::cerr << "Feed time: " << feedTime << std::endl;
    std::cerr << "Run time: " << runTimer.total << std::endl;
    std::cerr << std::endl;
#endif

    return 0;
}
