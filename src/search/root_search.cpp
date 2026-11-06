#include <algorithm>
#include <array>
#include <cassert>

#include "search/search_detail.hpp"
#include "search/worker.hpp"

namespace search {
namespace {

inline constexpr std::array<int, 20> HelperDepthSkipSize{
    1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
};
inline constexpr std::array<int, 20> HelperDepthSkipPhase{
    0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7,
};

} // namespace

// Main root search driver.
void Worker::search_root() {
    // Terminal root: return immediately when no legal root move exists.
    if (root_lines.empty()) {
        root_result = terminal_root_result();
        return;
    }

    // An exhausted budget must retain a legal fallback without completing a depth.
    if (is_main_worker())
        poll_search_limits();

    // Iterative deepening searches one completed depth at a time.
    for (int depth = 1; depth <= limits.depth && !stop_requested(); ++depth) {
        if (!should_search_root_depth(depth))
            continue;
        if (!search_root_depth(depth, root_result.value))
            break;
        if (is_main_worker())
            finish_timed_iteration();
        if (limits.has_mate_within_limit(root_result.value))
            break;
    }
}

bool Worker::should_search_root_depth(int depth) const noexcept {
    if (is_main_worker() || depth == 1)
        return true;

    // Stagger helper depths to diversify shared-TT work.
    const size_t index = static_cast<size_t>(worker_id - 1) % HelperDepthSkipSize.size();
    return ((depth + HelperDepthSkipPhase[index]) / HelperDepthSkipSize[index]) % 2 == 0;
}

// Root aspiration loop for a single depth.
bool Worker::search_root_depth(int depth, EvalValue previous_value) {
    detail::AspirationWindow window{depth, previous_value};

    while (!stop_requested()) {
        // Keep root order but clear stale attempt state.
        for (RootLine& line : root_lines) {
            line.reset_attempt();
        }

        // Search this attempt while retaining the nominal iteration depth.
        if (!search_root_window(window.retry_depth, depth, window.alpha, window.beta))
            return false;

        // Promote the best completed root line.
        std::stable_sort(root_lines.begin(), root_lines.end(), is_better_root_line);
        const RootLine& best_line = root_lines.front();
        assert(best_line.has_completed_depth());
        const EvalValue value = best_line.value;
        assert(value > -eval_value::inf && value < eval_value::inf);

        switch (window.accept_or_widen(value)) {
        case detail::AspirationWindow::Outcome::FailLow:  stats.aspiration_fail_low(); break;
        case detail::AspirationWindow::Outcome::FailHigh: stats.aspiration_fail_high(); break;
        case detail::AspirationWindow::Outcome::Accepted:
            // Window hit: accept and publish the completed depth.
            root_result = best_line;
            publish_root_snapshot();
            if (is_main_worker())
                report_root_progress(root_result);
            return true;
        }
    }

    return false;
}

// Fixed-window root pass. Caller owns attempt reset and result ordering.
bool Worker::search_root_window(int       search_depth,
                                int       nominal_depth,
                                EvalValue alpha,
                                EvalValue beta) {
    assert(!root_lines.empty());
    assert(search_depth >= 1);
    assert(nominal_depth >= search_depth);
    root_iteration_depth = nominal_depth;

    int  move_count  = 0;
    bool has_pv_move = false;

    // Preserve caller order for iterative deepening and aspiration retries.
    for (RootLine& line : root_lines) {
        assert(line.has_root_move());
        const Move root_move = line.root_move;
        assert(board.is_legal_pseudo_move(root_move));

        ++move_count;

        board.make(root_move, detail::prefetch_child_tt);
        ++search_ply;

        // Root PVS searches full-window until a root PV is established.
        // Scout later root moves and re-search only strict alpha improvements.
        PrincipalVariation child_pv;
        EvalValue          value;
        if (move_count == 1 || !has_pv_move) {
            value = -alphabeta<NodeType::Pv>(-beta, -alpha, search_depth - 1, &child_pv);
        } else {
            value = -alphabeta<NodeType::NonPv>(-alpha - 1, -alpha, search_depth - 1);
            if (!stop_requested() && value > alpha) {
                stats.pvs_research(search_ply);
                child_pv.clear();
                value = -alphabeta<NodeType::Pv>(-beta, -alpha, search_depth - 1, &child_pv);
            }
        }

        board.unmake();
        --search_ply;

        // Do not record partial root-line state after a stop.
        if (stop_requested())
            return false;

        line.complete(nominal_depth, value, child_pv);

        // Let aspiration handle the fail-high window miss.
        if (value >= beta)
            return true;

        if (value > alpha) {
            // A new root move improved alpha.
            if (is_main_worker())
                report_root_progress(line);

            alpha       = value;
            has_pv_move = true;
        } else if (move_count > 1 && has_pv_move && value == alpha && alpha > -eval_value::inf) {
            // Keep the full-window alpha raiser ordered first on scout ties.
            line.value = alpha - 1;
        }
    }

    return true;
}

} // namespace search
