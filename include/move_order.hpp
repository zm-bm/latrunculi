#pragma once

#include "board.hpp"
#include "heuristics.hpp"
#include "move.hpp"
#include "thread.hpp"
#include "tt.hpp"

class MoveOrder {
   public:
    MoveOrder(Board&, Heuristics&, int, Move = NullMove, Move = NullMove);
    U16 scoreMove(const Move& move) const;

   private:
    enum Priority : U16 {
        HASH_MOVE    = 1 << 15,
        PV_MOVE      = 1 << 14,
        PROM_MOVE    = 1 << 13,
        GOOD_CAPTURE = 1 << 12,
        KILLER_MOVE  = 1 << 11,
        HISTORY_MOVE = 1 << 10,
        BAD_CAPTURE  = 0,
    };

    Board& board;
    Heuristics& heuristics;
    Move pvMove;
    Move hashMove;
    int ply;
};

inline MoveOrder::MoveOrder(
    Board& board, Heuristics& heuristics, int ply, Move pvMove, Move hashMove)
    : board(board), heuristics(heuristics), ply(ply), pvMove(pvMove), hashMove(hashMove) {};

inline U16 MoveOrder::scoreMove(const Move& move) const {
    if (move == pvMove) return PV_MOVE;

    if (move == hashMove) return HASH_MOVE;

    if (move.type() == PROMOTION) return PROM_MOVE;

    if (board.isCapture(move)) {
        int seeScore = board.see(move);
        return (seeScore >= 0) ? GOOD_CAPTURE + seeScore : BAD_CAPTURE;
    }

    if (heuristics.killers.isKiller(move, ply)) return KILLER_MOVE;

    return heuristics.history.get(board.sideToMove(), move.from(), move.to());
}
