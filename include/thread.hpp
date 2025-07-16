#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include "board.hpp"
#include "constants.hpp"
#include "heuristics.hpp"
#include "pv.hpp"
#include "search_options.hpp"
#include "search_stats.hpp"
#include "thread_pool.hpp"
#include "types.hpp"
#include "uci.hpp"

class Thread {
   public:
    Thread() = delete;
    Thread(int, UCIProtocolHandler&, ThreadPool&);
    ~Thread();

    void start();
    void exit();
    void stop();
    void wait();
    void set(SearchOptions&, TimePoint startTime);

   private:
    // search state
    Board board;
    PrincipalVariation pv;
    Heuristics heuristics;
    SearchStats<> stats;
    SearchOptions options;
    TimePoint startTime;
    I64 allocatedTime;
    U64 nodes;
    int ply;
    int lastScore;
    std::string lastPV;

    // thread state
    std::mutex mutex;
    std::condition_variable condition;
    bool exitSignal{false};
    std::atomic<bool> runSignal{false};
    std::atomic<bool> stopSignal{false};
    const int threadId;

    UCIProtocolHandler& uciHandler;
    ThreadPool& threadPool;
    std::thread thread;

    // main thread loop
    void loop();

    // search.cpp
    int search();
    template <NodeType = NodeType::Root>
    int alphabeta(int alpha, int beta, int depth);
    int quiescence(int alpha, int beta);

    int search_DEPRECATED();
    template <NodeType = NodeType::Root>
    int alphabeta_DEPRECATED(int, int, int);
    int quiescence_DEPRECATED(int, int);

    // inline functions / helpers
    bool isMainThread() const;
    bool isTimeUp() const;
    void reportBestLine(int score, int depth, bool force = false);

    friend class SearchTest;
    friend class SearchBenchmark;
    friend class ThreadPoolTest;

    friend class ThreadPool;
    friend class MoveOrder;
    friend class Board;
};

inline bool Thread::isMainThread() const { return threadId == 0; }

inline bool Thread::isTimeUp() const {
    if (!isMainThread() || (nodes % NODE_INTERVAL != 0)) {
        return false;
    }

    auto totalNodes = threadPool.accumulate(&Thread::nodes);
    if (options.nodes != INT32_MAX && totalNodes >= options.nodes) {
        return true;
    }

    auto elapsedTime = std::chrono::duration_cast<Milliseconds>(Clock::now() - startTime);
    return elapsedTime.count() > allocatedTime;
}

inline void Thread::reportBestLine(int score, int depth, bool force) {
    if (isMainThread()) {
        auto pvStr = std::string(pv);

        if (!force && score == lastScore && pvStr == lastPV) {
            return;
        }
        lastScore = score;
        lastPV    = pvStr;

        auto totalNodes  = threadPool.accumulate(&Thread::nodes);
        auto elapsedTime = std::chrono::duration_cast<Milliseconds>(Clock::now() - startTime);
        auto bestLine    = UCIBestLine{score, depth, totalNodes, elapsedTime, pvStr};

        uciHandler.info(bestLine);
    }
}
