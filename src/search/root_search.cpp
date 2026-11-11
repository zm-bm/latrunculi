#include <algorithm>
#include <array>
#include <cassert>

#include "movegen/moves.hpp"
#include "search/tt.hpp"
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

void Worker::build_root_candidates() {
    root_candidates.clear();

    // Root candidates start in ordered-stream order.
    const auto context = history.context_for(board, 0);
    auto moves = movegen::MainSearchMoves(board, history, context);

    for (Move move = moves.next(); !move.is_null(); move = moves.next()) {
        if (limits.allows_root_move(move))
            root_candidates.push_back(RootLine{.root_move = move, .value = -scores::inf});
    }
}

RootLine Worker::terminal_root_result() const {
    assert(root_candidates.empty());

    return RootLine{
        .root_move = NULL_MOVE,
        .value = board.is_check() ? -scores::mate : scores::draw,
        .depth = 0,
        .completed = true,
    };
}

// Main root search driver.
void Worker::search_root() {
    // Terminal root: return immediately when no legal root move exists.
    if (root_candidates.empty()) {
        accepted_result = terminal_root_result();
        return;
    }

    // An exhausted budget must retain a legal fallback without completing a depth.
    if (is_main_worker())
        poll_search_limits();

    // Iterative deepening searches one completed depth at a time.
    for (int depth = 1; depth <= limits.depth && !stop_requested(); ++depth) {
        if (!should_search_root_depth(depth))
            continue;
        if (!search_root_depth(depth, accepted_result.value))
            break;
        if (is_main_worker())
            finish_timed_iteration();
        if (limits.has_mate_within_limit(accepted_result.value))
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
bool Worker::search_root_depth(int depth, Score previous_value) {
    assert(depth >= 1);

    Score delta = 50;
    Score alpha = std::max(previous_value - delta, -scores::inf);
    Score beta = std::min(previous_value + delta, scores::inf);
    int retry_depth = depth;

    while (!stop_requested()) {
        // Keep root order but clear stale attempt state.
        for (RootLine& line : root_candidates) {
            line.reset_attempt();
        }

        // Search this attempt while retaining the nominal iteration depth.
        if (!search_root_window(retry_depth, depth, alpha, beta))
            return false;

        // Promote the best completed root line.
        std::stable_sort(root_candidates.begin(), root_candidates.end(), is_better_root_line);
        const RootLine& best_line = root_candidates.front();
        assert(best_line.has_completed_depth());
        const Score value = best_line.value;
        assert(value > -scores::inf && value < scores::inf);

        if (value <= alpha) {
            // Fail low: recenter the lower bound and restore the nominal depth.
            stats.aspiration_fail_low();
            alpha = std::max(value - delta, -scores::inf);
            retry_depth = depth;
        } else if (value >= beta) {
            // Fail high: retry one ply shallower, except for mate-like scores.
            stats.aspiration_fail_high();
            beta = std::min(value + delta, scores::inf);
            retry_depth = value <= -scores::mate_bound || value >= scores::mate_bound
                ? depth
                : std::max(1, depth - 1);
        } else {
            // Window hit: accept and publish the completed depth.
            accepted_result = best_line;
            publish_root_snapshot();
            if (is_main_worker())
                report_root_progress(accepted_result);
            return true;
        }

        // Recenter with the current delta above, then grow it for the next miss.
        const Score growth = delta / 2;
        delta = delta >= scores::inf - growth ? scores::inf : delta + growth;
    }

    return false;
}

// Fixed-window root pass. Caller owns attempt reset and result ordering.
bool Worker::search_root_window(int search_depth, int nominal_depth, Score alpha, Score beta) {
    assert(!root_candidates.empty());
    assert(search_depth >= 1);
    assert(nominal_depth >= search_depth);
    root_iteration_depth = nominal_depth;

    int move_count = 0;
    bool has_pv_move = false;

    // Preserve caller order for iterative deepening and aspiration retries.
    for (RootLine& line : root_candidates) {
        assert(line.has_root_move());
        const Move root_move = line.root_move;
        assert(board.is_legal_pseudo_move(root_move));

        ++move_count;

        board.make(root_move, prefetch_child_tt);

        // Search the first root move at full window. Later moves can use a scout
        // even after a fail-low; verify strict alpha improvements at full window.
        SearchFrame* child = &frames[1];
        child->pv.clear();
        child->state.singular_search_active = false;
        Score value;
        if (move_count == 1) {
            child->state.pv_enabled = true;
            value = -alphabeta<NodeType::Pv>(-beta, -alpha, search_depth - 1, child);
        } else {
            child->state.pv_enabled = false;
            value = -alphabeta<NodeType::NonPv>(-alpha - 1, -alpha, search_depth - 1, child);
            if (!stop_requested() && value > alpha) {
                stats.pvs_research(child->ply);
                child->pv.clear();
                child->state.pv_enabled = true;
                value = -alphabeta<NodeType::Pv>(-beta, -alpha, search_depth - 1, child);
            }
        }

        board.unmake();

        // Do not record partial root-line state after a stop.
        if (stop_requested())
            return false;

        line.complete(nominal_depth, value, child->pv);

        // Let aspiration handle the fail-high window miss.
        if (value >= beta)
            return true;

        if (value > alpha) {
            // A new root move improved alpha.
            if (is_main_worker())
                report_root_progress(line);

            alpha = value;
            has_pv_move = true;
        } else if (move_count > 1 && has_pv_move && value == alpha && alpha > -scores::inf) {
            // Keep the full-window alpha raiser ordered first on scout ties.
            line.value = alpha - 1;
        }
    }

    return true;
}

} // namespace search
