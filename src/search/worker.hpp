#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <vector>

#include "board/board.hpp"
#include "core/types.hpp"
#include "search/instrumentation.hpp"
#include "search/limits.hpp"
#include "search/ordering/state.hpp"
#include "search/reporter.hpp"
#include "search/root_line.hpp"

class SearchTestAccess;

namespace search {

class ThreadPool;
class Thread;

enum class NodeType { Pv, NonPv };

// Per-thread search state and search execution.
class Worker {
public:
    Worker() = delete;
    Worker(int id, Reporter& reporter, ThreadPool& pool);
    Worker(const Worker&)            = delete;
    Worker& operator=(const Worker&) = delete;
    Worker(Worker&&)                 = delete;
    Worker& operator=(Worker&&)      = delete;

    // Thread-facing lifecycle.
    void      configure_search(const Board& root_board, Limits limits, TimePoint start_time);
    EvalValue search();
    void      request_stop() noexcept;

    // ThreadPool-facing progress and results.
    NodeCount node_count() const noexcept;
    RootLine  root_snapshot() const;

private:
    // Board and search state.
    Board                 board;
    int                   search_ply{0};
    RootLine              root_result;
    std::vector<RootLine> root_lines;
    ordering::State       ordering_state;

    // Current search request.
    Limits                      limits;
    TimePoint                   start_time{};
    std::optional<Milliseconds> allocated_time;

    // Progress and diagnostics.
    std::atomic<NodeCount> nodes{0};
    Instrumentation<>      stats;

    // Non-owning shared services. Both must outlive this worker.
    Reporter&   reporter;
    ThreadPool& thread_pool;
    const int   worker_id;

    // Stop state.
    std::atomic<bool> stop_requested_flag{false};

    // Root result snapshots.
    mutable std::mutex root_snapshot_mutex;
    RootLine           root_result_snapshot;

    // Search info reporting.
    std::optional<RootLine> last_reported_root_line;
    std::optional<Move>     pending_best_move;

    // Search lifecycle.
    bool      settle(Board& position);
    void      reset_search_state();
    void      clear_search_heuristics();
    void      wait_for_stop() const noexcept;
    void      build_root_lines();
    EvalValue search_root();
    RootLine  terminal_root_result() const;
    bool      search_root_depth(int depth, EvalValue previous_value);
    bool      search_root_window(int depth, EvalValue alpha, EvalValue beta);
    void      finalize_root_result(EvalValue value);
    void      prepare_final_result();
    void      publish_final_result();
    void      report_root_progress(const RootLine& line);

    // Root snapshot publication.
    void clear_root_snapshot();
    void publish_root_snapshot();

    // Search algorithm. (algorithm.cpp)
    template <NodeType Node = NodeType::NonPv>
    EvalValue alphabeta(EvalValue           alpha,
                        EvalValue           beta,
                        int                 depth,
                        PrincipalVariation* pv       = nullptr,
                        bool                can_null = true);
    template <NodeType Node = NodeType::NonPv, bool UseTt = true>
    EvalValue quiescence(EvalValue alpha, EvalValue beta, PrincipalVariation* pv = nullptr);

    // Accounting and limits.
    Milliseconds runtime() const;
    NodeCount    total_nodes() const;
    void         poll_search_limits();
    void         reset_nodes() noexcept;
    void         increment_nodes() noexcept;

    // Hot predicates.
    bool stop_requested() const noexcept;
    bool is_main_worker() const noexcept;
    bool should_search_root_depth(int depth) const noexcept;
    bool should_poll_search_limits() const noexcept;

    friend class ::SearchTestAccess;
    friend class Thread;
    friend class ThreadPool;
};

inline NodeCount Worker::node_count() const noexcept {
    return nodes.load(std::memory_order_relaxed);
}

inline void Worker::reset_nodes() noexcept {
    nodes.store(0, std::memory_order_relaxed);
}

inline void Worker::increment_nodes() noexcept {
    nodes.fetch_add(1, std::memory_order_relaxed);
}

inline bool Worker::stop_requested() const noexcept {
    return stop_requested_flag.load(std::memory_order_relaxed);
}

inline void Worker::request_stop() noexcept {
    stop_requested_flag.store(true, std::memory_order_relaxed);
    stop_requested_flag.notify_all();
}

inline void Worker::wait_for_stop() const noexcept {
    stop_requested_flag.wait(false, std::memory_order_relaxed);
}

inline bool Worker::is_main_worker() const noexcept {
    return worker_id == 0;
}

inline bool Worker::should_poll_search_limits() const noexcept {
    return is_main_worker() && ((node_count() & 0xFFF) == 0);
}

} // namespace search
