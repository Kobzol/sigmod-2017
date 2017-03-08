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
#include "nfa.h"
#include "timer.h"


static SimpleMap<std::string, DictHash>* wordMap;
static std::vector<Query>* queries;
static Nfa* nfa;
static std::vector<Word> words;

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
    Timer insertHashTimer;
    Timer nfaAddEdgeTimer;
    Timer createStateTimer;
    Timer addArcTimer;
    Timer getArcTimer;
    static double addCreateWord{0};
    static double addWordmap{0};
#endif

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
    std::unordered_set<int> found;
    size_t timestamp = query.timestamp;

    std::string& line = query.document;
    line += ' ';
    int size = (int) query.document.size();

    std::string ngram;
    NfaVisitor visitor;

    visitor.states[visitor.stateIndex].emplace_back(0, 0);

    for (int i = 2; i < size; i++)
    {
        char c = line[i];

        if (__builtin_expect(c == ' ', false))
        {
            for (auto& mapping : visitor.states[visitor.stateIndex])
            {
                CombinedNfaState& state = nfa->states[mapping.state];
                int wordIndex = state.wordIndex;
                if (wordIndex != NO_WORD)
                {
                    Word& word = words[wordIndex];
                    if (word.is_active(timestamp) && found.find(wordIndex) == found.end())
                    {
                        found.insert(wordIndex);
                        matches.emplace_back(i - word.length, word.length);
                    }
                }
            }
        }
        nfa->feedWord(visitor, c);

        if (__builtin_expect(c == ' ', false))
        {
            visitor.states[visitor.stateIndex].emplace_back(0, 0);
        }
    }

    std::sort(matches.begin(), matches.end(), [](Match& m1, Match& m2)
    {
        if (m1.index < m2.index) return true;
        else if (m1.index > m2.index) return false;
        else return m1.length <= m2.length;
    });

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
}

void delete_ngram(std::string& line, size_t timestamp)
{
    line[0] = 'A';
    DictHash index = wordMap->get(line);
    if (index != HASH_NOT_FOUND)
    {
        Word& ngram = words[index];
        if (ngram.is_active(timestamp))
        {
            ngram.deactivate(timestamp);
        }
    }
}
void add_ngram(const std::string& line, size_t timestamp)
{
    words.emplace_back(timestamp, line);
    size_t index = words.size() - 1;
    (*wordMap).insert(line, index);
    nfa->addWord(line, 2, index);
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
    computeTimer.start();
#endif
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
            words.emplace_back(0, "A " + line);
            size_t index = words.size() - 1;
            (*wordMap).insert("A " + line, index);
            nfa->addWord(line, 0, index);
        }
    }
}
void init(std::istream& input)
{
    wordMap = new SimpleMap<std::string, DictHash>(WORDMAP_HASH_SIZE);
    nfa = new Nfa();

    //omp_set_num_threads(THREAD_COUNT);
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

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
#endif
        }
        else batch(queryIndex);
    }

#ifdef PRINT_STATISTICS
    std::cerr << "Calc time: " << calcCount << std::endl;
    std::cerr << "Sort time: " << sortCount << std::endl;
    std::cerr << "Create result time: " << writeStringCount << std::endl;
    std::cerr << "IO time: " << ioTimer.total << std::endl;
    std::cerr << "Add time: " << addTimer.total << std::endl;
    std::cerr << "Add create word time: " << addCreateWord << std::endl;
    std::cerr << "Add wordmap time: " << addWordmap << std::endl;
    std::cerr << "Delete time: " << deleteTimer.total << std::endl;
    std::cerr << "Query time: " << queryTimer.total << std::endl;
    std::cerr << "Batch time: " << batchTimer.total << std::endl;
    std::cerr << "Split jobs time: " << splitJobsTimer.total << std::endl;
    std::cerr << "Find time: " << computeTimer.total << std::endl;
    std::cerr << "Write result time: " << writeResultTimer.total << std::endl;
    std::cerr << "Insert hash time: " << insertHashTimer.total << std::endl;
    std::cerr << "NFA add edge time: " << nfaAddEdgeTimer.total << std::endl;
    std::cerr << "NFA create state time: " << createStateTimer.total << std::endl;
    std::cerr << "NFA add arc time: " << addArcTimer.total << std::endl;
    std::cerr << "NFA get arc time: " << getArcTimer.total << std::endl;
    std::cerr << std::endl;
#endif

    return 0;
}
