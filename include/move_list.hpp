#pragma once

#include <algorithm>
#include <array>

#include "board.hpp"
#include "defs.hpp"
#include "history.hpp"
#include "killers.hpp"
#include "move.hpp"

struct MoveOrderContext {
    Board&        board;
    KillerMoves&  killers;
    HistoryTable& history;
    int           ply;
    Move          pv_move;
    Move          tt_move;
    bool          root      = false;
    int           thread_id = 0;
};

class MoveList {
public:
    MoveList() : last(moves.data()) {}

    const Move*  begin() const { return moves.begin(); }
    const Move*  end() { return last; }
    const bool   empty() { return last == moves.data(); }
    const size_t size() { return static_cast<std::size_t>(last - moves.data()); }

    Move& operator[](int index) { return moves[index]; }
    void  add(Square from, Square to, MoveType mtype = BASIC_MOVE, PieceType prom = KNIGHT);

    void     sort(const MoveOrderContext& ctx);
    uint16_t priority(const MoveOrderContext& ctx, const Move& move) const;

private:
    void diversify_root_order(const MoveOrderContext& ctx);

    std::array<Move, MAX_MOVES> moves;

    Move* last;
};

inline void MoveList::add(Square from, Square to, MoveType mtype, PieceType prom) {
    new (last++) Move(from, to, mtype, prom);
}

inline void MoveList::sort(const MoveOrderContext& ctx) {
    for (Move* move = moves.data(); move != last; ++move)
        move->priority = priority(ctx, *move);

    auto comp = [](const Move& a, const Move& b) { return a.priority > b.priority; };
    std::stable_sort(moves.begin(), last, comp);

    diversify_root_order(ctx);
}

inline void MoveList::diversify_root_order(const MoveOrderContext& ctx) {
    if (!ctx.root || ctx.thread_id == 0)
        return;

    for (Move* group_first = moves.data(); group_first != last;) {
        Move* group_last = group_first + 1;
        while (group_last != last && group_last->priority == group_first->priority)
            ++group_last;

        const auto group_size = group_last - group_first;
        if (group_size > 1 && group_first->priority < PRIORITY_HASH) {
            const auto rotation = ctx.thread_id % group_size;
            std::rotate(group_first, group_first + rotation, group_last);
        }

        group_first = group_last;
    }
}

inline uint16_t MoveList::priority(const MoveOrderContext& ctx, const Move& move) const {
    if (move == ctx.pv_move)
        return PRIORITY_PV;

    if (move == ctx.tt_move)
        return PRIORITY_HASH;

    if (move.type() == MOVE_PROM)
        return PRIORITY_PROM;

    if (ctx.board.is_capture(move)) {
        int seeScore = ctx.board.seeMove(move);
        return (seeScore >= 0) ? PRIORITY_CAPTURE + seeScore : PRIORITY_WEAK;
    }

    if (ctx.killers.is_killer(move, ctx.ply))
        return PRIORITY_KILLER;

    Color  c    = ctx.board.side_to_move();
    Square from = move.from();
    Square to   = move.to();

    return ctx.history.get(c, from, to);
}
