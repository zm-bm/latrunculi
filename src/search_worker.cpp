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
#include "uci.hpp"

SearchWorker::SearchWorker(int id, uci::Protocol& protocol, ThreadPool& pool)
    : board(position_states.root(), Board::startfen),
      protocol(protocol),
      thread_pool(pool),
      thread_id(id) {}

// Configuration.
void SearchWorker::configure_search(const SearchOptions& search_options) {
    board.bind_position_state(position_states.root());
    board.load_board(search_options.board);
    ply = 0;

    options       = search_options;
    searchtime_ms = options.calc_searchtime_ms(board.side_to_move());

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

    killers.clear();
    quiet_history.clear();

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
    MovePicker picker = MovePicker::main_search(board, killers, quiet_history, 0);
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
        .depth     = options.depth,
        .completed = true,
    };
}

void SearchWorker::record_root_result(int value) {
    if (!root_result.completed && !stop_requested()) {
        root_result.value     = value;
        root_result.depth     = options.depth;
        root_result.completed = true;
    }

    if (!root_result.completed)
        root_result.pv.clear();

    update_root_snapshot();
}

void SearchWorker::report_final_result() {
    thread_pool.stop_helper_searches();

    const RootLine selected = select_best_root_line(root_result, thread_pool.root_snapshots());

    protocol.info(uci::make_search_info(selected, board, total_nodes(), runtime()));
    protocol.bestmove(selected.root_move.str());

    if constexpr (SEARCH_STATS) {
        auto stats = thread_pool.aggregate_instrumentation();
        protocol.diagnostic_output(stats);
    }
}

// Accounting and limits.
Milliseconds SearchWorker::runtime() const {
    return std::chrono::duration_cast<Milliseconds>(Clock::now() - options.starttime);
}

uint64_t SearchWorker::total_nodes() const {
    return thread_pool.nodes_searched();
}

void SearchWorker::poll_search_limits() {
    if (options.nodes != OPTION_NOT_SET) {
        auto searched = total_nodes();
        if (searched >= options.nodes)
            thread_pool.request_stop();
    }

    if (searchtime_ms != OPTION_NOT_SET) {
        auto runtime_ms = runtime().count();
        if (runtime_ms >= searchtime_ms)
            thread_pool.request_stop();
    }
}
