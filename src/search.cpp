#include <algorithm>
#include <cassert>

#include "defs.hpp"
#include "evaluator.hpp"
#include "move_picker.hpp"
#include "search_worker.hpp"

int SearchWorker::search_root() {
    if (root_lines.empty()) {
        // A completed null best move represents root mate or stalemate.
        root.best_move = NULL_MOVE;
        root.value     = board.is_check() ? -MATE_VALUE : DRAW_VALUE;
        root.depth     = options.depth;
        root.completed = true;
        root.pv.clear();
        return root.value;
    }

    // Completed iterations seed the next root-move order. Partial iterations are discarded.
    for (int depth = 1; depth <= options.depth && !stop_requested(); ++depth) {
        if (!search_root_iteration(depth))
            break;

        std::stable_sort(root_lines.begin(), root_lines.end(), is_better_root_line);
        root = root_lines.front();
        publish_root_snapshot();
    }

    return root.value;
}

bool SearchWorker::search_root_iteration(int depth) {
    for (RootLine& line : root_lines) {
        Move move = line.best_move;
        assert(!move.is_null());
        assert(board.is_legal_generated_move(move));

        board.make(move, position_states.child(ply));
        ++ply;

        PrincipalVariation child_pv;
        const int          value = -alphabeta(-INF_VALUE, INF_VALUE, depth - 1, &child_pv);

        board.unmake(position_states.parent(ply));
        --ply;

        if (stop_requested())
            return false;

        line.value     = value;
        line.depth     = depth;
        line.completed = true;
        line.pv.update(move, child_pv);
    }

    return true;
}

int SearchWorker::alphabeta(int alpha, int beta, int depth, PrincipalVariation* pv) {
    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    // Terminal handling.
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
        return quiescence(alpha, beta, pv);

    increment_nodes();
    stats.node(ply);

    // Mate-distance pruning: bound impossible scores by root-relative ply.
    alpha = std::max(alpha, -MATE_VALUE + ply);
    beta  = std::min(beta, MATE_VALUE - ply - 1);
    if (alpha >= beta)
        return alpha;

    const bool         in_check         = board.is_check();
    const Color        turn             = board.side_to_move();
    int                legal_move_index = 0;
    int                best_value       = -INF_VALUE;
    MovePicker         picker{{board, killers, history, ply, NULL_MOVE, NULL_MOVE},
                      MovePickerMode::MainSearch};
    PrincipalVariation child_pv;

    if constexpr (SEARCH_STATS)
        stats.staged_node(ply, picker);

    // Main move loop. This foundation search still uses a full window for every legal move.
    for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
        if (!board.is_legal_generated_move(move))
            continue;

        ++legal_move_index;

        const bool is_promotion = move.type() == MOVE_PROM;
        const bool is_capture   = board.is_capture(move);
        const bool is_quiet     = !is_capture && !is_promotion;

        board.make(move, position_states.child(ply));
        ++ply;
        // Future scout/null-window searches should pass nullptr here.
        const int value = -alphabeta(-beta, -alpha, depth - 1, pv ? &child_pv : nullptr);
        board.unmake(position_states.parent(ply));
        --ply;

        if (stop_requested()) {
            if constexpr (SEARCH_STATS)
                stats.staged_generation(ply, picker);
            return alpha;
        }

        if (value >= beta) {
            if constexpr (SEARCH_STATS) {
                stats.staged_cutoff(ply, picker);
                stats.staged_generation(ply, picker);
            }

            if (is_quiet) {
                killers.update(move, ply);
                history.update(turn, move.from(), move.to(), depth);
            }

            if (pv)
                pv->update(move, child_pv);

            stats.beta_cutoff(ply, legal_move_index);
            return value;
        }

        if (value > best_value) {
            best_value = value;

            if (value > alpha) {
                alpha = value;
                if (pv)
                    pv->update(move, child_pv);
            }
        }
    }

    if constexpr (SEARCH_STATS)
        stats.staged_generation(ply, picker);

    // No legal moves means checkmate or stalemate.
    if (legal_move_index == 0)
        best_value = in_check ? -MATE_VALUE + ply : DRAW_VALUE;

    return best_value;
}

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

    const bool in_check         = board.is_check();
    int        legal_move_index = 0;
    int        best_value       = -INF_VALUE;

    // Stand-pat is only legal when the side to move is not in check.
    if (!in_check) {
        best_value = evaluate(board);
        if (best_value >= beta)
            return best_value;
        if (best_value > alpha)
            alpha = best_value;
    }

    MovePicker         picker{{board, killers, history, ply, NULL_MOVE, NULL_MOVE},
                      MovePickerMode::QSearch};
    PrincipalVariation child_pv;

    // Qsearch uses noisy moves outside check and full evasions while in check.
    for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
        if (!board.is_legal_generated_move(move))
            continue;

        ++legal_move_index;

        board.make(move, position_states.child(ply));
        ++ply;
        // Future scout/null-window qsearches should pass nullptr here.
        const int value = -quiescence(-beta, -alpha, pv ? &child_pv : nullptr);
        board.unmake(position_states.parent(ply));
        --ply;

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            if (pv)
                pv->update(move, child_pv);
            stats.beta_cutoff(ply, legal_move_index);
            return value;
        }

        if (value > best_value) {
            best_value = value;
            if (value > alpha) {
                alpha = value;
                if (pv)
                    pv->update(move, child_pv);
            }
        }
    }

    // In-check qsearch must find an evasion or it is checkmate.
    if (in_check && legal_move_index == 0)
        return -MATE_VALUE + ply;

    return best_value;
}
