#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "board.hpp"

#include "defs.hpp"
#include "history.hpp"
#include "killers.hpp"
#include "movegen.hpp"
#include "search_options.hpp"
#include "search_stats.hpp"
#include "tt.hpp"
#include "uci.hpp"

class ThreadPool;

struct RootSearchResult {
    Move     move{NULL_MOVE};
    int      value{DRAW_VALUE};
    int      depth{0};
    uint64_t nodes{0};
    bool     completed{false};
};

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

    std::array<PositionState, MAX_SEARCH_PLY + 1> position_states{};

    Board          board;
    int            ply{0};
    Move           root_move{NULL_MOVE};
    int            root_value{DRAW_VALUE};
    int            root_depth{0};
    KillerMoves    killers;
    HistoryTable   history;
    SearchOptions  options;
    int64_t        searchtime_ms{OPTION_NOT_SET};
    uint64_t       nodes{0};
    SearchStats<>  stats;
    uci::Protocol& protocol;
    ThreadPool&    thread_pool;

    // thread state

    const int               thread_id;
    mutable std::mutex      mutex;
    std::condition_variable cv;
    std::condition_variable done_cv;
    bool                    exit_flag{false};
    bool                    run_flag{false};
    bool                    busy_flag{false};
    std::atomic<bool>       search_started_flag{false};
    std::atomic<bool>       halt_flag{false};
    std::thread             worker;
    mutable std::mutex      root_result_mutex;
    RootSearchResult        root_result;

    // main thread loop

    void loop();

    // search.cpp

    void reset();
    int  search();
    int  search_widen(int depth, int value);
    template <NodeType = NON_PV>
    int quiescence(int alpha, int beta, int qply = 0);
    template <NodeType = ROOT>
    int alphabeta(int alpha, int beta, int depth, bool canNull = true);

    // inline helpers

    bool             is_busy() const;
    Milliseconds     get_runtime() const;
    uint64_t         get_nodes() const;
    std::string      get_pv(int depth) const;
    uci::PV          get_pv_line(int score, int depth) const;
    Move             first_legal_root_move() const;
    int              initial_search_depth() const;
    void             clear_root_result();
    void             publish_root_result();
    RootSearchResult root_result_snapshot() const;

    void set_options(SearchOptions& options);
    void check_halt_conditions();
    bool halt_requested() const { return halt_flag.load(std::memory_order_relaxed); }

    friend class SearchTest;
    friend class SearchBenchmark;
    friend class ThreadTest;
    friend class ThreadPoolTest;

    friend class ThreadPool;
};

inline bool Thread::is_busy() const {
    std::lock_guard<std::mutex> lock(mutex);
    return busy_flag;
}

inline void Thread::set_options(SearchOptions& options) {
    board.bind_position_state(position_states[0]);
    board.load_board(options.board);
    ply = 0;

    this->options = options;
    searchtime_ms = options.calc_searchtime_ms(board.side_to_move());
}

inline Milliseconds Thread::get_runtime() const {
    return std::chrono::duration_cast<Milliseconds>(Clock::now() - options.starttime);
}

inline std::string Thread::get_pv(int depth) const {
    std::array<PositionState, MAX_SEARCH_PLY + 1> pv_position_states{};
    Board                                         b{pv_position_states[0]};
    b.load_board(&board);

    std::string pv;
    for (int d = 1; d <= depth; ++d) {
        auto probe = tt.probe(b.key());

        if (!probe.has_value() || !b.is_legal_move(probe->move))
            break;

        b.make(probe->move, pv_position_states[d]);
        pv += probe->move.str() + " ";
    }

    return pv;
}

inline uci::PV Thread::get_pv_line(int score, int depth) const {
    return uci::PV{score, depth, get_nodes(), get_runtime(), get_pv(depth)};
}

inline Move Thread::first_legal_root_move() const {
    auto movelist = movegen::generate_pseudo_legal(board);
    for (auto& move : movelist) {
        if (board.is_legal_generated_move(move))
            return move;
    }
    return NULL_MOVE;
}
