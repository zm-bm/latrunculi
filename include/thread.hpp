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
#include "uci_output.hpp"

class Thread {
   public:
    Thread() = delete;
    Thread(int, UCIOutput&, ThreadPool&);
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
    int ply;

    // thread state
    std::mutex mutex;
    std::condition_variable condition;
    bool exitSignal{false};
    std::atomic<bool> runSignal{false};
    std::atomic<bool> stopSignal{false};
    const int threadId;

    UCIOutput& uciOutput;
    ThreadPool& threadPool;
    std::thread thread;

    // main thread loop
    void loop();

    // search.cpp
    int search();
    template <NodeType = NodeType::Root>
    int alphabeta(int, int, int);
    int quiescence(int, int);

    // inline functions / helpers
    bool isMainThread() const;
    Milliseconds getElapsedTime() const;
    bool isTimeUp() const;

    void uciInfo(int score, int depth, bool force = false) const;
    void uciBestMove() const;
    void uciDebugStats() const;

    friend class SearchTest;
    friend class SearchBenchmark;

    friend class ThreadPool;
    friend class MoveOrder;
    friend class Board;
};

inline bool Thread::isMainThread() const { return threadId == 0; }

inline Milliseconds Thread::getElapsedTime() const {
    return std::chrono::duration_cast<Milliseconds>(Clock::now() - startTime);
};

inline bool Thread::isTimeUp() const {
    auto elapsedTime = getElapsedTime();
    return elapsedTime.count() > options.movetime;
}

inline void Thread::uciInfo(int score, int depth, bool force) const {
    if (isMainThread()) {
        auto nodes       = threadPool.getNodeCount();
        auto elapsedTime = getElapsedTime();
        uciOutput.sendInfo(score, depth, nodes, elapsedTime, pv, force);
    }
}

inline void Thread::uciBestMove() const {
    if (isMainThread()) {
        auto bestMove = pv.bestMove();
        uciOutput.sendBestmove(bestMove.str());
    }
}

inline void Thread::uciDebugStats() const {
    if (isMainThread()) {
        auto stats = threadPool.getStats();
        uciOutput.sendStats(stats);
    }
}
