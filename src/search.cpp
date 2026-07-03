#include <algorithm>
#include <cassert>

#include "defs.hpp"
#include "evaluator.hpp"
#include "move_picker.hpp"
#include "search_worker.hpp"
#include "tt.hpp"

namespace {

// Aspiration-window defaults.
constexpr int AspirationWindow = 50;
constexpr int SearchInf        = static_cast<int>(INF_VALUE);

// Apply the PV/non-PV TT cutoff policy.
template <NodeType Node>
bool tt_cutoff_allowed(
    const TT_Record& record, int adjusted_score, int depth, int alpha, int beta) {
    if constexpr (Node == PV)
        return record.depth >= depth && record.flag == TT_Flag::Exact;

    return record.can_cutoff(adjusted_score, depth, alpha, beta);
}

} // namespace

// Main root search driver.
int SearchWorker::search_root() {
    // Handle mates and draws before any search attempts.
    if (root_lines.empty()) {
        root_result = terminal_root_result();
        return root_result.value;
    }

    // Iterative deepening loop.
    for (int depth = 1; depth <= options.depth && !stop_requested(); ++depth) {
        if (!search_root_depth(depth, root_result.value))
            break;
    }

    return root_result.value;
}

// Root aspiration loop for a single depth.
bool SearchWorker::search_root_depth(int depth, int previous_value) {
    int delta = AspirationWindow;
    int alpha = std::max(previous_value - delta, -SearchInf);
    int beta  = std::min(previous_value + delta, SearchInf);

    while (!stop_requested()) {
        // Reset per-attempt result state without disturbing root move order.
        for (RootLine& line : root_lines) {
            line.reset_attempt();
        }

        // Attempt a fixed-window search at this depth.
        if (!search_root_window(depth, alpha, beta))
            return false;

        // Sort the completed lines by value, then select the best usable line.
        std::stable_sort(root_lines.begin(), root_lines.end(), is_better_root_line);
        const RootLine& best_line = root_lines.front();
        assert(best_line.has_completed_depth());
        const int value = best_line.value;
        assert(value > -SearchInf && value < SearchInf);

        if (value <= alpha) {
            // Fail-low: widen only the lower side before retrying.
            stats.aspiration_fail_low();
            alpha = std::max(alpha - delta, -SearchInf);
            stats.aspiration_research();
        } else if (value >= beta) {
            // Fail-high: widen only the upper side before retrying.
            stats.aspiration_fail_high();
            beta = std::min(beta + delta, SearchInf);
            stats.aspiration_research();
        } else {
            // In-window result: accept and publish this depth.
            root_result = best_line;
            update_root_snapshot();
            return true;
        }

        // Widen the aspiration window for the next retry.
        delta = delta >= SearchInf / 2 ? SearchInf : delta * 2;
    }

    return false;
}

// Fixed-window root pass. Caller owns attempt reset and result ordering.
bool SearchWorker::search_root_window(int depth, int alpha, int beta) {
    assert(!root_lines.empty());

    int  move_count    = 0;
    bool pv_move_found = false;

    // Preserve the caller's root order; aspiration retries and ID depend on it.
    for (RootLine& line : root_lines) {
        assert(line.has_root_move());
        const Move root_move = line.root_move;
        assert(board.is_legal_generated_move(root_move));

        ++move_count;

        board.make(root_move, position_states.child(ply));
        ++ply;

        // Conservative root PVS: search full-window until a root PV is established,
        // then scout later moves and re-search only strict alpha improvements.
        PrincipalVariation child_pv;
        int                value;
        if (move_count == 1 || !pv_move_found) {
            value = -alphabeta<PV>(-beta, -alpha, depth - 1, &child_pv);
        } else {
            value = -alphabeta<NON_PV>(-alpha - 1, -alpha, depth - 1);
            if (!stop_requested() && value > alpha) {
                stats.pvs_research(ply);
                child_pv.clear();
                value = -alphabeta<PV>(-beta, -alpha, depth - 1, &child_pv);
            }
        }

        board.unmake(position_states.parent(ply));
        --ply;

        // Do not record partial root-line state from a stopped attempt.
        if (stop_requested())
            return false;

        line.complete(depth, value, child_pv);

        // Root fail-high ends this attempt; aspiration widens beta.
        if (value >= beta)
            return true;

        if (value > alpha) {
            // A new root PV has been found
            alpha         = value;
            pv_move_found = true;
        } else if (move_count > 1 && pv_move_found && value == alpha && alpha > -INF_VALUE) {
            // A later scout result equal to alpha is not a proven exact tie.
            // Demote its ordering score so the full-window line that raised alpha stays first.
            line.value = alpha - 1;
        }
    }

    return true;
}

// Recursive main search: alpha-beta for non-PV nodes, PVS for PV nodes.
template <NodeType Node>
int SearchWorker::alphabeta(int alpha, int beta, int depth, PrincipalVariation* pv) {
    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    // Resolve terminal and limit cases before move generation.
    const bool drawn = board.is_draw(ply);

    if (drawn) {
        increment_nodes();
        stats.node(ply);
        return DRAW_VALUE;
    }

    if (ply >= MAX_SEARCH_PLY) {
        increment_nodes();
        stats.node(ply);
        return evaluate(board);
    }

    if (depth <= 0)
        return quiescence<Node>(alpha, beta, pv);

    increment_nodes();
    stats.node(ply);

    // Mate-distance pruning: bound impossible scores by root-relative ply.
    alpha = std::max(alpha, -MATE_VALUE + ply);
    beta  = std::min(beta, MATE_VALUE - ply - 1);
    if (alpha >= beta)
        return alpha;

    const int      original_alpha = alpha;
    const uint64_t position_key   = board.key();
    Move           tt_move        = NULL_MOVE;

    // Probe for a cutoff or a hash move.
    stats.main_tt_probe(ply);
    if (auto record = tt.probe(position_key)) {
        stats.main_tt_hit(ply);

        const int tt_score = record->score_at_ply(ply);
        if (tt_cutoff_allowed<Node>(*record, tt_score, depth, alpha, beta)) {
            stats.main_tt_cutoff(ply);
            return tt_score;
        }

        tt_move = record->move;
    }

    const bool         in_check   = board.is_check();
    const Color        turn       = board.side_to_move();
    int                move_count = 0;
    int                best_value = -INF_VALUE;
    Move               best_move  = NULL_MOVE;
    MovePicker         picker     = MovePicker::main_search(board, killers, history, ply, tt_move);
    PrincipalVariation child_pv;

    // Search ordered legal moves.
    for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
        if (!board.is_legal_generated_move(move))
            continue;

        ++move_count;

        const bool is_promotion = move.type() == MOVE_PROM;
        const bool is_capture   = board.is_capture(move);
        const bool is_quiet     = !is_capture && !is_promotion;

        board.make(move, position_states.child(ply));
        ++ply;

        // PV nodes search the first move full-window; later moves scout first.
        int value;
        if constexpr (Node == NON_PV) {
            value = -alphabeta<NON_PV>(-beta, -alpha, depth - 1);
        } else if (move_count == 1) {
            value = -alphabeta<PV>(-beta, -alpha, depth - 1, pv ? &child_pv : nullptr);
        } else {
            value = -alphabeta<NON_PV>(-alpha - 1, -alpha, depth - 1);
            if (!stop_requested() && value > alpha) {
                stats.pvs_research(ply);
                child_pv.clear();
                value = -alphabeta<PV>(-beta, -alpha, depth - 1, pv ? &child_pv : nullptr);
            }
        }

        board.unmake(position_states.parent(ply));
        --ply;

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            // Beta cutoff: return fail-soft value and store a lower bound.
            if (is_quiet) {
                killers.update(move, ply);
                history.update(turn, move.from(), move.to(), depth);
            }

            if (pv)
                pv->update(move, child_pv);

            stats.beta_cutoff(ply, move_count);
            tt.store_search(position_key, move, value, depth, TT_Flag::Lowerbound, ply);
            return value;
        }

        if (value > best_value) {
            best_value = value;
            best_move  = move;

            if (value > alpha) {
                alpha = value;
                if (pv)
                    pv->update(move, child_pv);
            }
        }
    }

    // No legal moves means checkmate or stalemate.
    if (move_count == 0) {
        best_value = in_check ? -MATE_VALUE + ply : DRAW_VALUE;
        tt.store_search(position_key, NULL_MOVE, best_value, depth, TT_Flag::Exact, ply);
        return best_value;
    }

    // Store the best value with the bound implied by the original window.
    tt.store_search(position_key,
                    best_move,
                    best_value,
                    depth,
                    tt_bound_for_window(best_value, original_alpha, beta),
                    ply);

    return best_value;
}

// Quiescence search for tactical depth-zero nodes.
template <NodeType Node>
int SearchWorker::quiescence(int alpha, int beta, PrincipalVariation* pv) {
    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    increment_nodes();
    stats.qnode(ply);

    // Terminal handling. Qsearch is below root, so draw shortcuts are allowed.
    if (board.is_draw(ply))
        return DRAW_VALUE;

    if (ply >= MAX_SEARCH_PLY)
        return evaluate(board);

    constexpr int  qsearch_tt_depth = 0;
    const int      original_alpha   = alpha;
    const uint64_t position_key     = board.key();
    Move           tt_move          = NULL_MOVE;

    // Use depth-0 TT records with the same PV cutoff policy.
    stats.q_tt_probe(ply);
    if (auto record = tt.probe(position_key)) {
        stats.q_tt_hit(ply);

        const int tt_score = record->score_at_ply(ply);
        if (tt_cutoff_allowed<Node>(*record, tt_score, qsearch_tt_depth, alpha, beta)) {
            stats.q_tt_cutoff(ply);
            return tt_score;
        }

        tt_move = record->move;
    }

    const bool in_check   = board.is_check();
    int        move_count = 0;
    int        best_value = -INF_VALUE;
    Move       best_move  = NULL_MOVE;

    // Stand-pat supplies the initial lower bound when not in check.
    if (!in_check) {
        best_value = evaluate(board);
        if (best_value >= beta) {
            tt.store_search(
                position_key, NULL_MOVE, best_value, qsearch_tt_depth, TT_Flag::Lowerbound, ply);
            return best_value;
        }
        if (best_value > alpha)
            alpha = best_value;
    }

    MovePicker         picker = MovePicker::qsearch(board, history, tt_move);
    PrincipalVariation child_pv;

    // Search noisy moves unless in check, where all evasions are required.
    for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
        if (!board.is_legal_generated_move(move))
            continue;

        ++move_count;

        board.make(move, position_states.child(ply));
        ++ply;
        const int value = -quiescence<Node>(-beta, -alpha, pv ? &child_pv : nullptr);
        board.unmake(position_states.parent(ply));
        --ply;

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            if (pv)
                pv->update(move, child_pv);
            stats.beta_cutoff(ply, move_count);
            tt.store_search(position_key, move, value, qsearch_tt_depth, TT_Flag::Lowerbound, ply);
            return value;
        }

        if (value > best_value) {
            best_value = value;
            best_move  = move;
            if (value > alpha) {
                alpha = value;
                if (pv)
                    pv->update(move, child_pv);
            }
        }
    }

    // In-check qsearch must find an evasion or it is checkmate.
    if (in_check && move_count == 0) {
        best_value = -MATE_VALUE + ply;
        tt.store_search(position_key, NULL_MOVE, best_value, qsearch_tt_depth, TT_Flag::Exact, ply);
        return best_value;
    }

    tt.store_search(position_key,
                    best_move,
                    best_value,
                    qsearch_tt_depth,
                    tt_bound_for_window(best_value, original_alpha, beta),
                    ply);

    return best_value;
}

// Template definitions live in this translation unit; instantiate the node types we use.
template int SearchWorker::alphabeta<PV>(int, int, int, PrincipalVariation*);
template int SearchWorker::alphabeta<NON_PV>(int, int, int, PrincipalVariation*);
template int SearchWorker::quiescence<PV>(int, int, PrincipalVariation*);
template int SearchWorker::quiescence<NON_PV>(int, int, PrincipalVariation*);
