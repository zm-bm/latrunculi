#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "board.hpp"
#include "constants.hpp"
#include "context.hpp"
#include "heuristics.hpp"
#include "pv.hpp"
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
    void set(const std::string&, Context&, TimePoint startTime = Clock::now());

    Board board;
    PrincipalVariation pv;
    Heuristics heuristics;
    Statistics stats;
    int ply;

    Context context;
    TimePoint startTime;

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

    void startAll(Board&, Context&);
    void stopAll();
    void waitAll();
    Statistics aggregateStats() const;

    static inline std::atomic<bool> stopSignal{false};

    friend class SearchBenchmark;

   private:
    std::vector<std::unique_ptr<Thread>> threads;
};
