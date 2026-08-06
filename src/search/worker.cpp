#include <atomic>
#include <cassert>
#include <chrono>
#include <mutex>

#include "board/board.hpp"
#include "eval/evaluator.hpp"
#include "search/ordering/picker.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"
#include "search/worker.hpp"

namespace search {

Worker::Worker(int id, Reporter& reporter, ThreadPool& pool)
    : reporter(reporter),
      thread_pool(pool),
      worker_id(id) {}

// Configuration.
void Worker::configure_search(const Board& root_board, Limits limits, TimePoint search_start_time) {
    board      = root_board;
    search_ply = 0;

    this->limits   = limits;
    start_time     = search_start_time;
    allocated_time = this->limits.allocated_time(board.side_to_move());

    reset_nodes();
    clear_root_snapshot();
    stop_requested_flag.store(false, std::memory_order_relaxed);
}

// Root snapshot publication.
void Worker::clear_root_snapshot() {
    std::lock_guard<std::mutex> lock(root_snapshot_mutex);
    root_result_snapshot = {};
}

void Worker::publish_root_snapshot() {
    std::lock_guard<std::mutex> lock(root_snapshot_mutex);
    root_result_snapshot = root_result;
}

RootLine Worker::root_snapshot() const {
    std::lock_guard<std::mutex> lock(root_snapshot_mutex);
    return root_result_snapshot;
}

// Search lifecycle.
EvalValue Worker::search() {
    reset_search_state();

    if (is_main_worker()) {
        tt.advance_generation();
        thread_pool.release_helper_searches();
    } else {
        thread_pool.wait_for_helper_release();
    }

    build_root_lines();
    const EvalValue value = search_root();

    if (is_main_worker()) {
        // Ponderhit leaves only ponder mode; an explicit infinite request still
        // requires stop before final publication.
        if (limits.ponder)
            thread_pool.wait_while_pondering();
        if (limits.infinite && !stop_requested())
            wait_for_stop();
    }

    finalize_root_result(value);

    if (is_main_worker())
        prepare_final_result();

    return root_result.value;
}

void Worker::reset_search_state() {
    reset_nodes();
    search_ply  = 0;
    root_result = RootLine{NULL_MOVE, evaluate(board), 0, false};
    root_lines.clear();
    last_reported_root_line.reset();
    pending_best_move.reset();

    ordering_state.prepare_for_search();

    if constexpr (stats_enabled)
        stats.reset();
}

void Worker::clear_search_heuristics() {
    ordering_state.clear();
}

void Worker::build_root_lines() {
    root_lines.clear();

    // Root candidates start in the current picker order.
    const auto context = ordering::State::make_context(board);
    auto       picker  = ordering::Picker::for_main_search(board, ordering_state, context, 0);

    for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
        if (board.is_legal_pseudo_move(move) && limits.allows_root_move(move))
            root_lines.push_back(RootLine{.root_move = move, .value = -eval_value::inf});
    }
}

// Root result reporting.
RootLine Worker::terminal_root_result() const {
    assert(root_lines.empty());

    return RootLine{
        .root_move = NULL_MOVE,
        .value     = board.is_check() ? -eval_value::mate : eval_value::draw,
        .depth     = 0,
        .completed = true,
    };
}

void Worker::finalize_root_result(EvalValue value) {
    if (!root_result.completed && !stop_requested()) {
        root_result.value     = value;
        root_result.depth     = limits.depth;
        root_result.completed = true;
    }

    if (!root_result.completed)
        root_result.pv.clear();

    publish_root_snapshot();
}

void Worker::prepare_final_result() {
    thread_pool.stop_helper_searches();

    RootLine selected = root_result;

    // Preserve a proven mate instead of replacing it with a deeper helper
    // result that does not satisfy the requested mate limit.
    if (!limits.has_mate_within_limit(selected.value))
        selected = select_best_root_line(selected, thread_pool.root_snapshots());

    // A stopped depth-zero search still needs a legal fallback move.
    if (!selected.usable_root_move() && !root_lines.empty()) {
        selected.root_move = root_lines.front().root_move;
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
    if (last_reported_root_line && line == *last_reported_root_line)
        return;

    reporter.report_progress(line, board, total_nodes(), runtime());
    last_reported_root_line = line;
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

    if (allocated_time) {
        if (runtime() >= *allocated_time)
            thread_pool.request_stop();
    }
}

} // namespace search
