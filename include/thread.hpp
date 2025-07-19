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
    PVTable pvTable;
    SearchOptions options;
    TimePoint startTime;
    I64 searchTime;
    int ply;

    // heuristics
    KillerMoves killers;
    HistoryTable history;

    // search stats
    U64 nodes;
    SearchStats<> stats;

    // search results
    PVLine rootLine;
    int rootValue;
    int rootDepth;

    // references
    UCIProtocolHandler& uciHandler;
    ThreadPool& threadPool;

    // thread state
    std::mutex mutex;
    std::condition_variable condition;
    bool exitSignal{false};
    std::atomic<bool> runSignal{false};
    std::atomic<bool> stopSignal{false};
    const int threadId;
    std::thread thread;

    // main thread loop
    void loop();

    // search.cpp
    void reset();
    int search();
    int searchWiden(int depth, int value);

    template <NodeType = NodeType::Root>
    int alphabeta(int alpha, int beta, int depth);

    int quiescence(int alpha, int beta);

    int search_DEPRECATED();
    template <NodeType>
    int alphabeta_DEPRECATED(int, int, int);
    int quiescence_DEPRECATED(int, int);

    // inline functions / helpers
    bool isMainThread() const;
    void checkStop();
    bool isTimeUp_DEPRECATED() const;

    void sendInfo(int score, int depth, PVLine& line);
    void sendMove(Move move);
    void sendStats();

    friend class SearchTest;
    friend class SearchBenchmark;
    friend class ThreadPoolTest;

    friend class ThreadPool;
    friend class MoveOrder;
    friend class Board;
};

inline bool Thread::isMainThread() const { return threadId == 0; }

inline void Thread::checkStop() {
    if ((nodes & 0xFFF) != 0) return;
    if (!isMainThread()) return;

    bool stopSearch = false;

    if (options.nodes != OPTION_NOT_SET) {
        auto totalNodes = threadPool.accumulate(&Thread::nodes);
        stopSearch      = totalNodes >= options.nodes;
    }

    else if (searchTime != OPTION_NOT_SET) {
        auto elapsedTime = std::chrono::duration_cast<Milliseconds>(Clock::now() - startTime);
        stopSearch       = elapsedTime.count() > searchTime;
    }

    if (stopSearch) threadPool.stopAll();
}

inline bool Thread::isTimeUp_DEPRECATED() const {
    if (!isMainThread() || (nodes % 1024 != 0)) {
        return false;
    }

    auto totalNodes = threadPool.accumulate(&Thread::nodes);
    if (options.nodes != INT32_MAX && totalNodes >= options.nodes) {
        return true;
    }

    auto elapsedTime = std::chrono::duration_cast<Milliseconds>(Clock::now() - startTime);
    return elapsedTime.count() > searchTime;
}

inline void Thread::sendInfo(int score, int depth, PVLine& line) {
    if (isMainThread()) {
        auto pv          = toString(line);
        auto totalNodes  = threadPool.accumulate(&Thread::nodes);
        auto elapsedTime = std::chrono::duration_cast<Milliseconds>(Clock::now() - startTime);

        auto bestLine = UCIBestLine{score, depth, totalNodes, elapsedTime, pv};

        uciHandler.info(bestLine);
    }
}

inline void Thread::sendMove(Move move) { uciHandler.bestmove(move.str()); }

inline void Thread::sendStats() {
    auto stats = threadPool.accumulate(&Thread::stats);
    uciHandler.logOutput(stats);
}
