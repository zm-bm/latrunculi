#include <algorithm>

#include "defs.hpp"
#include "evaluator.hpp"
#include "move_picker.hpp"
#include "search_worker.hpp"

int SearchWorker::search_root() {
    return alphabeta<ROOT>(-INF_VALUE, INF_VALUE, options.depth, &root.pv);
}

template <NodeType N>
int SearchWorker::alphabeta(int alpha, int beta, int depth, PrincipalVariation* pv) {
    constexpr bool root_node = N == ROOT;

    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    increment_nodes();
    stats.node(ply);

    // Terminal handling. Root draw shortcuts are skipped so UCI gets a best move.
    const bool drawn = !root_node && board.is_draw(ply);

    if (drawn)
        return DRAW_VALUE;

    if (ply >= MAX_SEARCH_PLY)
        return evaluate(board);

    if (depth <= 0)
        return evaluate(board);

    if constexpr (!root_node) {
        // Mate-distance pruning: bound impossible scores by root-relative ply.
        alpha = std::max(alpha, -MATE_VALUE + ply);
        beta  = std::min(beta, MATE_VALUE - ply - 1);
        if (alpha >= beta)
            return alpha;
    }

    const bool         in_check         = board.is_check();
    const Color        turn             = board.side_to_move();
    int                legal_move_index = 0;
    int                best_value       = -INF_VALUE;
    Move               best_move        = NULL_MOVE;
    MovePicker         picker{{board, killers, history, ply, NULL_MOVE, NULL_MOVE},
                      MovePickerMode::MainSearch};
    PrincipalVariation child_pv;

    if constexpr (SEARCH_STATS)
        stats.staged_node(ply, picker);

    // Main move loop. S0 searches every legal move with the full window.
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
        const int value = -alphabeta<NON_PV>(-beta, -alpha, depth - 1, pv ? &child_pv : nullptr);
        board.unmake(position_states.parent(ply));
        --ply;

        if (stop_requested()) {
            if constexpr (SEARCH_STATS)
                stats.staged_generation(ply, picker);
            if constexpr (root_node)
                return root.value;
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

            if constexpr (root_node) {
                root.best_move = move;
                root.value     = value;
                root.depth     = depth;
            }

            if (pv)
                pv->update(move, child_pv);

            stats.beta_cutoff(ply, legal_move_index);
            return value;
        }

        if (value > best_value) {
            best_value = value;
            best_move  = move;

            if (value > alpha) {
                alpha = value;
                if (pv)
                    pv->update(move, child_pv);

                if constexpr (root_node) {
                    root.value     = value;
                    root.depth     = depth;
                    root.best_move = move;
                }
            }
        }
    }

    if constexpr (SEARCH_STATS)
        stats.staged_generation(ply, picker);

    // No legal moves means checkmate or stalemate.
    if (legal_move_index == 0) {
        best_move  = NULL_MOVE;
        best_value = in_check ? -MATE_VALUE + ply : DRAW_VALUE;
    }

    if constexpr (root_node) {
        root.best_move = best_move;
        root.value     = best_value;
        root.depth     = depth;
        if (best_move.is_null())
            root.pv.clear();
    }

    return best_value;
}

template int SearchWorker::alphabeta<ROOT>(int alpha, int beta, int depth, PrincipalVariation* pv);
template int
SearchWorker::alphabeta<NON_PV>(int alpha, int beta, int depth, PrincipalVariation* pv);
