#include <atomic>
#include <cassert>
#include <chrono>
#include <mutex>

#include "board/board.hpp"
#include "eval/evaluation.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"
#include "search/worker.hpp"

namespace search {

Worker::Worker(int id, Reporter& reporter, ThreadPool& pool)
    : reporter(reporter),
      thread_pool(pool),
      worker_id(id) {
    for (int ply = 0; ply <= engine::max_search_ply; ++ply)
        frames[ply].ply = ply;
}

// Configuration.
void Worker::configure_search(const Board& root_board, Limits limits, TimePoint search_start_time) {
    board = root_board;

    this->limits = limits;
    start_time   = search_start_time;
    time_manager = TimeManager{this->limits, board.side_to_move()};

    reset_nodes();
    clear_root_snapshot();
    stop_requested_flag.store(false, std::memory_order_relaxed);
}

// Root snapshot publication.
void Worker::clear_root_snapshot() {
    std::lock_guard<std::mutex> lock(published_result_mutex);
    published_result = {};
}

void Worker::publish_root_snapshot() {
    std::lock_guard<std::mutex> lock(published_result_mutex);
    published_result = accepted_result;
}

RootLine Worker::root_snapshot() const {
    std::lock_guard<std::mutex> lock(published_result_mutex);
    return published_result;
}

// Search lifecycle.
Score Worker::search() {
    reset_search_state();

    if (is_main_worker()) {
        tt.advance_generation();
        thread_pool.start_helper_searches();
    }

    build_root_candidates();
    search_root();

    if (is_main_worker()) {
        // Ponderhit leaves only ponder mode; an explicit infinite request still
        // requires stop before final publication.
        if (limits.ponder)
            thread_pool.wait_while_pondering();
        if (limits.infinite && !stop_requested())
            wait_for_stop();
    }

    if (!accepted_result.completed && !stop_requested()) {
        accepted_result.depth     = limits.depth;
        accepted_result.completed = true;
    }

    if (!accepted_result.completed)
        accepted_result.pv.clear();

    publish_root_snapshot();

    if (is_main_worker())
        prepare_final_result();

    return accepted_result.value;
}

void Worker::reset_search_state() {
    reset_nodes();
    root_iteration_depth = 0;
    accepted_result      = RootLine{NULL_MOVE, eval::evaluate(board), 0, false};
    for (auto& frame : frames) {
        frame.static_eval.reset();
        frame.excluded_move          = NULL_MOVE;
        frame.real_move              = false;
        frame.pv_enabled             = false;
        frame.singular_search_active = false;
        frame.pv.clear();
    }
    if (!board.is_check())
        frames[0].static_eval = accepted_result.value;
    root_candidates.clear();
    last_reported_result.reset();
    pending_best_move.reset();

    history.prepare_for_search();

    if constexpr (stats_enabled)
        stats.reset();
}

void Worker::clear_search_heuristics() {
    history.clear();
}

// Root result reporting.
void Worker::prepare_final_result() {
    thread_pool.stop_helper_searches();

    RootLine selected = accepted_result;

    // Preserve a proven mate instead of replacing it with a deeper helper
    // result that does not satisfy the requested mate limit.
    if (!limits.has_mate_within_limit(selected.value))
        selected = thread_pool.select_root_result(selected);

    // A stopped depth-zero search still needs a legal fallback move.
    if (!selected.usable_root_move() && !root_candidates.empty()) {
        selected.root_move = root_candidates.front().root_move;
        selected.depth     = 0;
        selected.completed = false;
        selected.pv.clear();
    }

    reporter.report_progress(selected, board, total_nodes(), runtime());
    pending_best_move = selected.root_move;
}

void Worker::publish_final_result() {
    if (!pending_best_move)
        return;

    reporter.report_best_move(*pending_best_move);
    pending_best_move.reset();

    if constexpr (stats_enabled) {
        auto stats = thread_pool.aggregate_instrumentation();
        reporter.report_diagnostic(stats.str());
    }
}

void Worker::report_root_progress(const RootLine& line) {
    if (last_reported_result && line == *last_reported_result)
        return;

    reporter.report_progress(line, board, total_nodes(), runtime());
    last_reported_result = line;
}

// Accounting and limits.
Milliseconds Worker::runtime() const {
    return std::chrono::duration_cast<Milliseconds>(SearchClock::now() - start_time);
}

NodeCount Worker::total_nodes() const {
    return thread_pool.nodes_searched();
}

void Worker::poll_search_limits() {
    // Ponder work still accumulates, but cannot stop the search before ponderhit.
    if (limits.infinite || thread_pool.is_pondering())
        return;

    if (limits.nodes) {
        auto searched = total_nodes();
        if (searched >= *limits.nodes)
            thread_pool.request_stop();
    }

    if (const auto maximum = time_manager.hard_limit()) {
        if (runtime() >= *maximum)
            thread_pool.request_stop();
    }
}

void Worker::finish_timed_iteration() {
    assert(is_main_worker() && accepted_result.usable_root_move());

    if (!time_manager.soft_limit() || thread_pool.is_pondering())
        return;

    time_manager.update_after_iteration(
        accepted_result.depth, accepted_result.root_move, accepted_result.value);
    if (root_candidates.size() == 1 || runtime() >= *time_manager.soft_limit())
        thread_pool.request_stop();
}

} // namespace search
