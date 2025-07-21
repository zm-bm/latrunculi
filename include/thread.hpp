#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include "board.hpp"
#include "constants.hpp"
#include "heuristics.hpp"
#include "search_options.hpp"
#include "search_stats.hpp"
#include "thread_pool.hpp"
#include "tt.hpp"
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
    Move rootMove;
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
    int alphabeta(int alpha, int beta, int depth, bool canNull = DO_NULL);

    int quiescence(int alpha, int beta);

    int search_DEPRECATED();
    template <NodeType>
    int alphabeta_DEPRECATED(int, int, int);
    int quiescence_DEPRECATED(int, int);

    // inline functions / helpers
    void checkStop();
    bool isTimeUp_DEPRECATED() const;

    std::string getPV(int depth) const;
    UCIBestLine getBestLine(int score, int depth) const;

    friend class SearchTest;
    friend class SearchBenchmark;
    friend class ThreadPoolTest;

    friend class ThreadPool;
    friend class MoveOrder;
    friend class Board;
};

inline void Thread::checkStop() {
    if ((nodes & 0xFFF) != 0) return;
    if (threadId != 0) return;

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

inline std::string Thread::getPV(int depth) const {
    std::string pv;
    Board b{board.toFEN()};

    for (int d = 1; d <= depth; ++d) {
        TT_Entry* e = tt.probe(b.getKey());

        if (e == nullptr || e->move == NullMove || !b.isLegalMove(e->move)) {
            break;
        }

        b.make(e->move);
        pv += e->move.str() + " ";
    }

    return pv;
}

inline UCIBestLine Thread::getBestLine(int score, int depth) const {
    auto totalNodes  = threadPool.accumulate(&Thread::nodes);
    auto elapsedTime = std::chrono::duration_cast<Milliseconds>(Clock::now() - startTime);
    auto pv          = getPV(depth);

    return UCIBestLine{score, depth, totalNodes, elapsedTime, pv};
}
