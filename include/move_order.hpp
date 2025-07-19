#pragma once

#include "board.hpp"
#include "heuristics.hpp"
#include "move.hpp"
#include "thread.hpp"
#include "tt.hpp"

class MoveOrder {
   public:
    MoveOrder(Board& board,
              int ply,
              KillerMoves& killers,
              HistoryTable& history,
              Move pvMove   = NullMove,
              Move hashMove = NullMove)
        : board(board),
          ply(ply),
          killers(killers),
          history(history),
          pvMove(pvMove),
          hashMove(hashMove) {}

    U16 scoreMove(const Move& move) const;

   private:
    Board& board;
    int ply;
    KillerMoves& killers;
    HistoryTable& history;
    Move pvMove;
    Move hashMove;
};

inline U16 MoveOrder::scoreMove(const Move& move) const {
    if (move == pvMove) return PV_MOVE;

    if (move == hashMove) return HASH_MOVE;

    if (move.type() == MoveType::Promotion) return PROM_MOVE;

    if (board.isCapture(move)) {
        int seeScore = board.see(move);
        return (seeScore >= 0) ? GOOD_CAPTURE + seeScore : BAD_CAPTURE;
    }

    if (killers.isKiller(move, ply)) return KILLER_MOVE;

    Color c     = board.sideToMove();
    Square from = move.from();
    Square to   = move.to();

    return history.get(c, from, to);
}
