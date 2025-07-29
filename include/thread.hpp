#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include "board.hpp"

#include "defs.hpp"
#include "history.hpp"
#include "killers.hpp"
#include "search_options.hpp"
#include "search_stats.hpp"
#include "thread_pool.hpp"
#include "tt.hpp"
#include "uci.hpp"

class Thread {
public:
    Thread() = delete;
    Thread(int id, uci::Protocol& protocol, ThreadPool& pool);
    ~Thread();

    void start(SearchOptions& options);
    void exit();
    void stop();
    void wait();

private:
    // search state

    Board          board;
    int            ply;
    Move           root_move;
    int            root_value;
    int            root_depth;
    KillerMoves    killers;
    HistoryTable   history;
    SearchOptions  options;
    int64_t        searchtime_ms;
    uint64_t       nodes;
    SearchStats<>  stats;
    uci::Protocol& protocol;
    ThreadPool&    thread_pool;

    // thread state

    std::mutex              mutex;
    std::condition_variable condition;
    bool                    exit_signal{false};
    std::atomic<bool>       run_signal{false};
    std::atomic<bool>       stop_signal{false};
    const int               thread_id;
    std::thread             thread;

    // main thread loop
    void loop();

    // search.cpp
    void reset();
    int  search();
    int  search_widen(int depth, int value);
    int  quiescence(int alpha, int beta);
    template <NodeType = ROOT>
    int alphabeta(int alpha, int beta, int depth, bool canNull = true);

    // inline helpers
    Milliseconds get_runtime() const;
    uint64_t     get_nodes() const;
    std::string  get_pv(int depth) const;
    uci::PV      get_pv_line(int score, int depth) const;
    void         check_stop();

    friend class SearchTest;
    friend class SearchBenchmark;
    friend class ThreadPoolTest;

    friend class ThreadPool;
    friend class Board;
};

inline Milliseconds Thread::get_runtime() const {
    return std::chrono::duration_cast<Milliseconds>(Clock::now() - options.starttime);
}

inline uint64_t Thread::get_nodes() const {
    return thread_pool.accumulate(&Thread::nodes);
}

inline std::string Thread::get_pv(int depth) const {
    Board b{board.toFEN()};

    std::string pv;
    for (int d = 1; d <= depth; ++d) {
        TT_Entry* e = tt.probe(b.key());

        if (e == nullptr || e->move.is_null() || !b.is_legal_move(e->move))
            break;

        b.make(e->move);
        pv += e->move.str() + " ";
    }

    return pv;
}

inline uci::PV Thread::get_pv_line(int score, int depth) const {
    return uci::PV{score, depth, get_nodes(), get_runtime(), get_pv(depth)};
}

inline void Thread::check_stop() {
    if (thread_id != 0)
        return;

    if (nodes & 0xFFF)
        return;

    if (options.nodes != OPTION_NOT_SET) {
        auto total_nodes = get_nodes();
        if (total_nodes >= options.nodes)
            thread_pool.stop_all();
    }

    if (searchtime_ms != OPTION_NOT_SET) {
        auto runtime_ms = get_runtime().count();
        if (runtime_ms >= searchtime_ms)
            thread_pool.stop_all();
    }
}
