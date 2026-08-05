#include <atomic>
#include <cassert>
#include <chrono>
#include <mutex>

#include "board/board.hpp"
#include "eval/evaluator.hpp"
#include "search/move_picker.hpp"
#include "search/search_worker.hpp"
#include "search/tt.hpp"
#include "uci/threading.hpp"
#include "uci/uci_writer.hpp"

SearchWorker::SearchWorker(int id, uci::Writer& writer, ThreadPool& pool)
    : writer(writer),
      thread_pool(pool),
      worker_id(id) {}

// Configuration.
void SearchWorker::configure_search(const Board& root_board,
                                    SearchLimits limits,
                                    TimePoint    search_start_time) {
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
void SearchWorker::clear_root_snapshot() {
    std::lock_guard<std::mutex> lock(root_snapshot_mutex);
    root_result_snapshot = {};
}

void SearchWorker::publish_root_snapshot() {
    std::lock_guard<std::mutex> lock(root_snapshot_mutex);
    root_result_snapshot = root_result;
}

RootLine SearchWorker::root_snapshot() const {
    std::lock_guard<std::mutex> lock(root_snapshot_mutex);
    return root_result_snapshot;
}

// Search lifecycle.
EvalValue SearchWorker::search() {
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

void SearchWorker::reset_search_state() {
    reset_nodes();
    search_ply  = 0;
    root_result = RootLine{NULL_MOVE, evaluate(board), 0, false};
    root_lines.clear();
    last_reported_root_line.reset();
    pending_bestmove.reset();

    ordering.prepare_for_search();

    if constexpr (SearchStatsEnabled)
        stats.reset();
}

void SearchWorker::clear_search_heuristics() {
    ordering.clear();
}

void SearchWorker::build_root_lines() {
    root_lines.clear();

    // Root candidates start in the current picker order.
    const auto context = MoveOrdering::make_context(board);
    auto       picker  = move_picker::main_search(board, ordering, context, 0);

    for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
        if (board.is_legal_pseudo_move(move) && limits.allows_root_move(move))
            root_lines.push_back(RootLine{.root_move = move, .value = -eval_value::inf});
    }
}

// Root result reporting.
RootLine SearchWorker::terminal_root_result() const {
    assert(root_lines.empty());

    return RootLine{
        .root_move = NULL_MOVE,
        .value     = board.is_check() ? -eval_value::mate : eval_value::draw,
        .depth     = 0,
        .completed = true,
    };
}

void SearchWorker::finalize_root_result(EvalValue value) {
    if (!root_result.completed && !stop_requested()) {
        root_result.value     = value;
        root_result.depth     = limits.depth;
        root_result.completed = true;
    }

    if (!root_result.completed)
        root_result.pv.clear();

    publish_root_snapshot();
}

void SearchWorker::prepare_final_result() {
    thread_pool.stop_helper_searches();

    RootLine selected = select_best_root_line(root_result, thread_pool.root_snapshots());

    // A stopped depth-zero search still owes UCI a legal move.
    if (!selected.usable_root_move() && !root_lines.empty()) {
        selected.root_move = root_lines.front().root_move;
        selected.depth     = 0;
        selected.completed = false;
        selected.pv.clear();
    }

    writer.search_info(selected, board, total_nodes(), runtime());
    pending_bestmove = selected.root_move;
}

void SearchWorker::publish_final_result() {
    if (!pending_bestmove)
        return;

    writer.bestmove(*pending_bestmove);
    pending_bestmove.reset();

    if constexpr (SearchStatsEnabled) {
        auto stats = thread_pool.aggregate_instrumentation();
        writer.debug(stats.str());
    }
}

void SearchWorker::report_root_progress(const RootLine& line) {
    if (last_reported_root_line && line == *last_reported_root_line)
        return;

    writer.search_info(line, board, total_nodes(), runtime());
    last_reported_root_line = line;
}

// Accounting and limits.
Milliseconds SearchWorker::runtime() const {
    return std::chrono::duration_cast<Milliseconds>(SearchClock::now() - start_time);
}

NodeCount SearchWorker::total_nodes() const {
    return thread_pool.nodes_searched();
}

void SearchWorker::poll_search_limits() {
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
