#include <atomic>
#include <cassert>
#include <chrono>
#include <mutex>

#include "board.hpp"
#include "evaluator.hpp"
#include "move_picker.hpp"
#include "search_worker.hpp"
#include "threading.hpp"
#include "tt.hpp"
#include "uci_writer.hpp"

SearchWorker::SearchWorker(int id, uci::Writer& writer, ThreadPool& pool)
    : board(position_states.root(), Board::startfen),
      writer(writer),
      thread_pool(pool),
      thread_id(id) {}

// Configuration.
void SearchWorker::configure_search(const Board& root_board,
                                    SearchLimits limits,
                                    TimePoint    search_start_time) {
    board.bind_position_state(position_states.root());
    board.load_board(&root_board);
    ply = 0;

    this->limits   = limits;
    start_time     = search_start_time;
    allocated_time = this->limits.allocated_time(board.side_to_move());

    clear_root_snapshot();
    stop_requested_flag.store(false, std::memory_order_relaxed);
}

// Root snapshot publication.
void SearchWorker::clear_root_snapshot() {
    std::lock_guard<std::mutex> lock(root_snapshot_mutex);
    root_result_snapshot = {};
}

void SearchWorker::update_root_snapshot() {
    std::lock_guard<std::mutex> lock(root_snapshot_mutex);
    root_result_snapshot = root_result;
}

RootLine SearchWorker::root_snapshot() const {
    std::lock_guard<std::mutex> lock(root_snapshot_mutex);
    return root_result_snapshot;
}

// Search lifecycle.
int SearchWorker::search() {
    reset_search_state();

    if (is_main_worker()) {
        prepare_shared_search_state();
        thread_pool.release_helper_searches();
    } else {
        thread_pool.wait_for_helper_release();
    }

    build_root_lines();
    const int value = search_root();

    record_root_result(value);

    if (is_main_worker())
        report_final_result();

    return root_result.value;
}

void SearchWorker::reset_search_state() {
    reset_nodes();
    ply         = 0;
    root_result = RootLine{NULL_MOVE, evaluate(board), 0, false};
    root_lines.clear();
    last_reported_root_line.reset();

    ordering.clear();

    if constexpr (SEARCH_STATS)
        stats.reset();
}

void SearchWorker::prepare_shared_search_state() {
    assert(is_main_worker());
    tt.age_table();
}

void SearchWorker::build_root_lines() {
    root_lines.clear();

    // Root candidates start in the current MovePicker order.
    const auto ctx    = MoveOrdering::make_context(board);
    MovePicker picker = MovePicker::main_search(board, ordering, ctx, 0);

    for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
        if (board.is_legal_generated_move(move))
            root_lines.push_back(RootLine{.root_move = move, .value = -INF_VALUE});
    }
}

// Root result reporting.
RootLine SearchWorker::terminal_root_result() const {
    assert(root_lines.empty());

    return RootLine{
        .root_move = NULL_MOVE,
        .value     = board.is_check() ? -MATE_VALUE : DRAW_VALUE,
        .depth     = limits.depth,
        .completed = true,
    };
}

void SearchWorker::record_root_result(int value) {
    if (!root_result.completed && !stop_requested()) {
        root_result.value     = value;
        root_result.depth     = limits.depth;
        root_result.completed = true;
    }

    if (!root_result.completed)
        root_result.pv.clear();

    update_root_snapshot();
}

void SearchWorker::report_final_result() {
    thread_pool.stop_helper_searches();

    const RootLine selected = select_best_root_line(root_result, thread_pool.root_snapshots());

    report_root_progress(selected);

    writer.bestmove(selected.root_move);

    if constexpr (SEARCH_STATS) {
        auto stats = thread_pool.aggregate_instrumentation();
        writer.debug(stats);
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
    return std::chrono::duration_cast<Milliseconds>(Clock::now() - start_time);
}

uint64_t SearchWorker::total_nodes() const {
    return thread_pool.nodes_searched();
}

void SearchWorker::poll_search_limits() {
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
