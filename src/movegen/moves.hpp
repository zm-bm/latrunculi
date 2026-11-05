#pragma once

#include "board/board.hpp"
#include "movegen/move_sources.hpp"

namespace movegen {

// A single-pass legal move stream. Restore the node's board before advancing.
// Main-search streams borrow a named OrderingContext which must outlive the stream.
// Selection precedes legality so ordered modes retain their swaps and tie order.
template <MovePolicy Policy>
class Moves {
public:
    explicit Moves(const Board& board) requires(Policy == MovePolicy::Unordered)
        : source(board) {}

    explicit Moves(const Board&                   board,
                   const search::History&         history,
                   const search::OrderingContext& context)
        requires(Policy == MovePolicy::MainSearch)
        : source(board, history, context) {}

    // The stream borrows context. Reject temporaries that would be destroyed
    // at the end of the construction expression, before traversal begins.
    Moves(const Board&, const search::History&, const search::OrderingContext&&)
        requires(Policy == MovePolicy::MainSearch)
        = delete;

    explicit Moves(const Board& board, const search::History& history, Move tt_move = NULL_MOVE)
        requires(Policy == MovePolicy::Quiescence)
        : source(board, history, tt_move) {}

    Moves(const Moves&)            = delete;
    Moves(Moves&&)                 = delete;
    Moves& operator=(const Moves&) = delete;
    Moves& operator=(Moves&&)      = delete;

    Move next() {
        return find([](Move) { return true; });
    }

    // Consume through the match, checking cheap predicates before legality (e.g. UCI).
    template <typename Match>
    Move find(Match match) {
        for (Move move = source.next_candidate(); !move.is_null(); move = source.next_candidate())
            if (match(move) && source.board.is_legal_pseudo_move(move))
                return move;
        return NULL_MOVE;
    }

    void skip_nonchecking_quiets() requires(Policy == MovePolicy::MainSearch)
    {
        source.skip_nonchecking_quiets();
    }

private:
    MoveSource<Policy> source;
};

// Named policies keep call sites explicit while sharing one traversal interface.
using UnorderedMoves  = Moves<MovePolicy::Unordered>;
using MainSearchMoves = Moves<MovePolicy::MainSearch>;
using QuiescenceMoves = Moves<MovePolicy::Quiescence>;

} // namespace movegen
