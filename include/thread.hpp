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
#include "tt.hpp"
#include "uci.hpp"

class ThreadPool;

class Thread {
public:
    Thread() = delete;
    Thread(int id, uci::Protocol& protocol, ThreadPool& pool);
    ~Thread();

    void start(SearchOptions& options);
    void halt();
    void wait();
    void shutdown();

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

    const int               thread_id;
    std::mutex              mutex;
    std::condition_variable cv;
    std::condition_variable done_cv;
    bool                    exit_flag{false};
    bool                    run_flag{false};
    bool                    busy_flag{false};
    std::atomic<bool>       halt_flag{false};
    std::thread             worker;

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

    void set_options(SearchOptions& options);
    void check_halt_conditions();
    bool halt_requested() const { return halt_flag.load(std::memory_order_relaxed); }

    friend class SearchTest;
    friend class SearchBenchmark;
    friend class ThreadPoolTest;

    friend class ThreadPool;
    friend class Board;
};

inline void Thread::set_options(SearchOptions& options) {
    board.load_board(options.board);

    this->options       = options;
    this->searchtime_ms = options.calc_searchtime_ms(board.side_to_move());
}

inline Milliseconds Thread::get_runtime() const {
    return std::chrono::duration_cast<Milliseconds>(Clock::now() - options.starttime);
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
