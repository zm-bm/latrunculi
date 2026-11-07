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
#include "search/reporter.hpp"
#include "search/root_line.hpp"
#include "search/search_frame.hpp"
#include "search/time_manager.hpp"

class SearchTestAccess;

namespace search {

class ThreadPool;
class Thread;

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
    void  configure_search(const Board& root_board, Limits limits, TimePoint start_time);
    Score search();
    void  request_stop() noexcept;

    // ThreadPool-facing progress and results.
    NodeCount node_count() const noexcept;
    RootLine  root_snapshot() const;

private:
    // Current position.
    Board board;

    // Root-search state: current candidates and the last accepted result.
    int                   root_iteration_depth{0};
    RootLine              accepted_result;
    std::vector<RootLine> root_candidates;

    // Persistent ordering memory.
    History history;

    // Include the maximum-ply entry so terminal calls have a valid frame and PV.
    // Default-initialize metadata without zeroing unused PV move slots.
    std::array<SearchFrame, engine::max_search_ply + 1> frames;

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

    // Search control state.
    std::atomic<bool> stop_requested_flag{false};
    // Active in a singular search and its descendants; prevents multicuts.
    // SearchFrame::excluded_move excludes a move only at its own ply.
    bool singular_search_active{false};

    // Synchronized result publication.
    mutable std::mutex published_result_mutex;
    RootLine           published_result;

    // Reporting state for this search.
    std::optional<RootLine> last_reported_result;
    std::optional<Move>     pending_best_move;

    // Search lifecycle.
    void reset_search_state();
    void clear_search_heuristics();
    void wait_for_stop() const noexcept;

    // Root search. (root_search.cpp)
    void     build_root_candidates();
    RootLine terminal_root_result() const;
    void     search_root();
    bool     should_search_root_depth(int depth) const noexcept;
    bool     search_root_depth(int depth, Score previous_value);
    bool     search_root_window(int search_depth, int nominal_depth, Score alpha, Score beta);

    // Recursive search. (search.cpp)
    bool settle(Board& position);
    template <NodeType Node = NodeType::NonPv>
    Score alphabeta(Score alpha, Score beta, int depth, SearchFrame* frame);
    template <NodeType Node = NodeType::NonPv, bool UseTt = true>
    Score quiescence(Score alpha, Score beta, SearchFrame* frame);

    // Root results and reporting. (worker.cpp)
    void prepare_final_result();
    void publish_final_result();
    void report_root_progress(const RootLine& line);
    void clear_root_snapshot();
    void publish_root_snapshot();

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
