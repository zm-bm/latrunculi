#include <atomic>
#include <chrono>
#include <mutex>

#include "board.hpp"
#include "evaluator.hpp"
#include "search_worker.hpp"
#include "threading.hpp"
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
    published_root = {};
}

void SearchWorker::publish_root_snapshot() {
    std::lock_guard<std::mutex> lock(root_snapshot_mutex);
    published_root = root;
}

RootLine SearchWorker::root_snapshot() const {
    std::lock_guard<std::mutex> lock(root_snapshot_mutex);
    return published_root;
}

// Search lifecycle.
int SearchWorker::search() {
    reset_search_state();

    if (is_main_worker()) {
        // Future root-move setup belongs before helper release.
        thread_pool.release_helper_searches();
    } else {
        thread_pool.wait_for_helper_release();
    }

    const int value = search_root();

    record_root_result(value);

    if (is_main_worker())
        report_final_result();

    return root.value;
}

void SearchWorker::reset_search_state() {
    reset_nodes();
    ply  = 0;
    root = RootLine{NULL_MOVE, evaluate(board), 0, false};

    killers.clear();
    history.clear();

    if constexpr (SEARCH_STATS)
        stats.reset();
}

// Root result reporting.
void SearchWorker::record_root_result(int value) {
    if (!stop_requested()) {
        root.value     = value;
        root.depth     = options.depth;
        root.completed = true;
    } else {
        root.pv.clear();
    }

    publish_root_snapshot();
}

void SearchWorker::report_final_result() {
    thread_pool.stop_helper_searches();

    const RootLine selected = select_best_root_line(root, thread_pool.root_lines());

    protocol.info(uci::make_search_info(selected, board, total_nodes(), runtime()));
    protocol.bestmove(selected.best_move.str());

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
