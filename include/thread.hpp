#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "board.hpp"
#include "constants.hpp"
#include "heuristics.hpp"
#include "options.hpp"
#include "stats.hpp"
#include "types.hpp"

// forward declare
class Engine;

struct PrincipalVariation {
    using Line = std::vector<Move>;

    std::array<Line, MAX_DEPTH> lines;

    Line& operator[](const int ply) { return lines[ply]; }
    Move bestMove() const { return !lines[0].empty() ? lines[0][0] : NullMove; }

    void update(const int ply, const Move& move) {
        Line& line = lines[ply];
        Line& prev = lines[ply + 1];

        line.clear();
        line.push_back(move);
        line.insert(line.end(), prev.begin(), prev.end());
    }

    void clear() {
        for (auto& line : lines) {
            line.clear();
        }
    }
};

class Thread {
   public:
    Thread(int id, Engine* engine);
    ~Thread();

    void start();
    void stop();
    void wait();
    void set(const std::string&, SearchContext&);

    Board board;
    PrincipalVariation pv;
    Heuristics heuristics;
    int ply;

    SearchContext context;
    SearchStats stats;

   private:
    // thread.cpp
    void loop();
    void reset();
    bool isMainThread();

    // search.cpp
    int search();
    template <NodeType = NodeType::Root>
    int alphabeta(int, int, int);
    int quiescence(int, int);
    void checkTime();

    std::mutex mutex;
    std::condition_variable condition;

    std::atomic<bool> exitSignal{false};
    std::atomic<bool> runSignal{false};
    const int threadId;

    std::thread thread;
    Engine* engine;

    friend class SearchTest;
    friend class SearchBenchmark;
};

class ThreadPool {
   public:
    ThreadPool(size_t numThreads, Engine* engine);
    ~ThreadPool();

    void startAll(Board&, SearchContext&);
    void stopAll();
    void waitAll();

    SearchStats aggregateStats() const;

    static inline std::atomic<bool> stopSignal{false};

    friend class SearchBenchmark;

   private:
    std::vector<std::unique_ptr<Thread>> threads;
};
