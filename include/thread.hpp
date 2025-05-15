#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "board.hpp"
#include "constants.hpp"
#include "heuristics.hpp"
#include "pv.hpp"
#include "searchoptions.hpp"
#include "statistics.hpp"
#include "types.hpp"

// forward declare
class Engine;

class Thread {
   public:
    Thread(int id, Engine* engine);
    ~Thread();

    void start();
    void stop();
    void wait();
    void set(const std::string&, SearchOptions&, TimePoint startTime = Clock::now());

   private:
    Board board;
    PrincipalVariation pv;
    Heuristics heuristics;
    Statistics stats;
    SearchOptions options;
    TimePoint startTime;
    int ply;

    std::mutex mutex;
    std::condition_variable condition;

    std::atomic<bool> exitSignal{false};
    std::atomic<bool> runSignal{false};
    const int threadId;

    std::thread thread;
    Engine* engine;

    // thread.cpp
    void loop();
    void reset();

    // search.cpp
    int search();
    template <NodeType = NodeType::Root>
    int alphabeta(int, int, int);
    int quiescence(int, int);

    // inline / helper functions
    bool isMainThread();
    Milliseconds elapsedTime() const;
    bool isTimeUp() const;

    friend class SearchTest;
    friend class SearchBenchmark;

    friend class ThreadPool;
    friend class MovePriority;
    friend class Board;
};

inline bool Thread::isMainThread() { return threadId == 0; }

inline Milliseconds Thread::elapsedTime() const {
    return std::chrono::duration_cast<Milliseconds>(Clock::now() - startTime);
}

inline bool Thread::isTimeUp() const { return elapsedTime().count() > options.movetime; }

class ThreadPool {
   public:
    ThreadPool(size_t numThreads, Engine* engine);
    ~ThreadPool();

    void startAll(Board&, SearchOptions&);
    void stopAll();
    void waitAll();
    Statistics aggregateStats() const;

    static inline std::atomic<bool> stopSignal{false};

    friend class SearchBenchmark;

   private:
    std::vector<std::unique_ptr<Thread>> threads;
};
