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
    void set(SearchOptions&, TimePoint startTime);

   private:
    // search state
    Board board;
    PrincipalVariation pv;
    Heuristics heuristics;
    Statistics stats;
    SearchOptions options;
    TimePoint startTime;
    int ply;

    // thread state
    std::mutex mutex;
    std::condition_variable condition;
    bool exitSignal{false};
    bool runSignal{false};
    const int threadId;

    // engine pointer for uci output
    Engine* engine;

    // standard library thread
    std::thread thread;

    // thread.cpp
    void loop();
    void reset();

    // search.cpp
    int search();
    template <NodeType = NodeType::Root>
    int alphabeta(int, int, int);
    int quiescence(int, int);

    // inline / helper functions
    Milliseconds elapsedTime() const;
    bool isTimeUp() const;
    bool isMainThread();

    friend class SearchTest;
    friend class SearchBenchmark;

    friend class ThreadPool;
    friend class MovePriority;
    friend class Board;
};

inline Milliseconds Thread::elapsedTime() const {
    return std::chrono::duration_cast<Milliseconds>(Clock::now() - startTime);
}

inline bool Thread::isTimeUp() const { return elapsedTime().count() > options.movetime; }

inline bool Thread::isMainThread() { return threadId == 0; }

class ThreadPool {
   public:
    ThreadPool(size_t numThreads, Engine* engine);
    ~ThreadPool();

    void startAll(SearchOptions&);
    void stopAll();
    void waitAll();
    Statistics aggregateStats() const;

    static inline std::atomic<bool> stopSignal{false};

   private:
    std::vector<std::unique_ptr<Thread>> threads;
};
