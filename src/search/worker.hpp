#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <optional>
#include <vector>

#include "board/board.hpp"
#include "core/types.hpp"
#include "search/history.hpp"
#include "search/instrumentation.hpp"
#include "search/limits.hpp"
#include "search/node_type.hpp"
#include "search/reporter.hpp"
#include "search/root_line.hpp"
#include "search/search_frame.hpp"
#include "search/time_manager.hpp"

class SearchTestAccess;

namespace search {

class ThreadPool;
class Thread;
struct TTRecord;

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
    Board board;
    int   root_iteration_depth{0};
    // Active throughout a verification call, including descendants and nested verification.
    bool                  verification_active{false};
    RootLine              root_result;
    std::vector<RootLine> root_lines;
    History               history;

    std::array<SearchFrame, engine::max_search_ply> frames{};

    // Current search request.
    Limits      limits;
    TimePoint   start_time{};
    TimeManager time_manager;

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
    void reset_search_state();
    void clear_search_heuristics();
    void wait_for_stop() const noexcept;

    // Root search. (root_search.cpp)
    void search_root();
    bool should_search_root_depth(int depth) const noexcept;
    bool search_root_depth(int depth, EvalValue previous_value);
    bool search_root_window(int search_depth, int nominal_depth, EvalValue alpha, EvalValue beta);

    // Recursive search. (search.cpp)
    bool settle(Board& position);
    template <NodeType Node = NodeType::NonPv>
    EvalValue alphabeta(
        EvalValue alpha, EvalValue beta, int depth, int ply, PrincipalVariation* pv = nullptr);
    template <NodeType Node = NodeType::NonPv, bool UseTt = true>
    EvalValue
    quiescence(EvalValue alpha, EvalValue beta, int ply, PrincipalVariation* pv = nullptr);

    // Singular verification and multicuts. (singular.cpp)
    EvalValue
    search_excluding_move(EvalValue alpha, EvalValue beta, int depth, int ply, Move excluded_move);
    struct SingularResult {
        bool                     extend{false};
        std::optional<EvalValue> cutoff;
    };
    template <NodeType Node>
    SingularResult
    singular_decision(const TTRecord& record, int depth, int ply, EvalValue alpha, EvalValue beta);

    // Root results and reporting. (worker.cpp)
    void     build_root_lines();
    RootLine terminal_root_result() const;
    void     prepare_final_result();
    void     publish_final_result();
    void     report_root_progress(const RootLine& line);
    void     clear_root_snapshot();
    void     publish_root_snapshot();

    // Accounting and limits.
    Milliseconds runtime() const;
    NodeCount    total_nodes() const;
    void         poll_search_limits();
    void         finish_timed_iteration();
    void         reset_nodes() noexcept;
    void         increment_nodes() noexcept;

    // Hot predicates.
    bool stop_requested() const noexcept;
    bool is_main_worker() const noexcept;
    bool should_poll_search_limits() const noexcept;
    bool after_null_move(int ply) const noexcept;

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

inline bool Worker::after_null_move(int ply) const noexcept {
    // Skip razoring, reverse futility, NMP, and singular verification at the null-search root.
    // Move-loop pruning and TT behavior remain ordinary; a real child restores all heuristics.
    // A loaded position without reversible history is not a null move.
    return ply > 0 && board.previous_move().is_null() && board.can_unmake();
}

} // namespace search
