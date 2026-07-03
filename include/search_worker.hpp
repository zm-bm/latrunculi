#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include "board.hpp"
#include "defs.hpp"
#include "history.hpp"
#include "killers.hpp"
#include "position_state.hpp"
#include "root_line.hpp"
#include "search_instrumentation.hpp"
#include "search_options.hpp"

class ThreadPool;
class ThreadTestAccess;

namespace uci {
class Protocol;
}

// Per-thread search state and search execution.
class SearchWorker {
public:
    SearchWorker() = delete;
    SearchWorker(int id, uci::Protocol& protocol, ThreadPool& pool);

    // Thread-facing lifecycle.
    void configure_search(const SearchOptions& options);
    int  search();
    void request_stop() noexcept;

    // ThreadPool-facing progress and results.
    uint64_t node_count() const noexcept;
    RootLine root_snapshot() const;

private:
    // Board and search state.
    PositionStateStack    position_states;
    Board                 board;
    int                   ply{0};
    RootLine              root_result;
    std::vector<RootLine> root_lines;
    KillerMoves           killers;
    HistoryTable          history;

    // Current search request.
    SearchOptions options;
    int64_t       searchtime_ms{OPTION_NOT_SET};

    // Progress and diagnostics.
    std::atomic<uint64_t>   nodes{0};
    SearchInstrumentation<> stats;

    // Shared services.
    uci::Protocol& protocol;
    ThreadPool&    thread_pool;
    const int      thread_id;

    // Stop state.
    std::atomic<bool> stop_requested_flag{false};

    // Root result snapshots.
    mutable std::mutex root_snapshot_mutex;
    RootLine           root_result_snapshot;

    // Search lifecycle.
    void     reset_search_state();
    void     prepare_shared_search_state();
    void     build_root_lines();
    int      search_root();
    RootLine terminal_root_result() const;
    bool     search_root_depth(int depth, int previous_value);
    bool     search_root_window(int depth, int alpha, int beta);
    void     record_root_result(int value);
    void     report_final_result();

    // Root snapshot publication.
    void clear_root_snapshot();
    void update_root_snapshot();

    // Search algorithm. (search.cpp)
    template <NodeType Node = NON_PV>
    int alphabeta(
        int alpha, int beta, int depth, PrincipalVariation* pv = nullptr, bool can_null = true);
    template <NodeType Node = NON_PV>
    int quiescence(int alpha, int beta, PrincipalVariation* pv = nullptr);

    // Accounting and limits.
    Milliseconds runtime() const;
    uint64_t     total_nodes() const;
    void         poll_search_limits();
    void         reset_nodes() noexcept;
    void         increment_nodes() noexcept;

    // Hot predicates.
    bool stop_requested() const noexcept;
    bool is_main_worker() const noexcept;
    bool should_poll_search_limits() const noexcept;

    friend class SearchTest;
    friend class ThreadPool;
    friend class ::ThreadTestAccess;
};

inline uint64_t SearchWorker::node_count() const noexcept {
    return nodes.load(std::memory_order_relaxed);
}

inline void SearchWorker::reset_nodes() noexcept {
    nodes.store(0, std::memory_order_relaxed);
}

inline void SearchWorker::increment_nodes() noexcept {
    nodes.fetch_add(1, std::memory_order_relaxed);
}

inline bool SearchWorker::stop_requested() const noexcept {
    return stop_requested_flag.load(std::memory_order_relaxed);
}

inline void SearchWorker::request_stop() noexcept {
    stop_requested_flag.store(true, std::memory_order_relaxed);
}

inline bool SearchWorker::is_main_worker() const noexcept {
    return thread_id == 0;
}

inline bool SearchWorker::should_poll_search_limits() const noexcept {
    return is_main_worker() && ((node_count() & 0xFFF) == 0);
}
