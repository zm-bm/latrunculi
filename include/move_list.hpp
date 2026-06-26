#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <utility>

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
    MoveList(const MoveList& other);
    MoveList(MoveList&& other) noexcept;

    MoveList& operator=(const MoveList& other);
    MoveList& operator=(MoveList&& other) noexcept;

    Move*       begin() { return moves.data(); }
    const Move* begin() const { return moves.data(); }
    Move*       end() { return last; }
    const Move* end() const { return last; }
    bool        empty() const { return last == moves.data(); }
    std::size_t size() const { return static_cast<std::size_t>(last - moves.data()); }

    Move&       operator[](int index) { return moves[index]; }
    const Move& operator[](int index) const { return moves[index]; }
    void        add(Square from, Square to, MoveType mtype = BASIC_MOVE, PieceType prom = KNIGHT);

    void     sort(const MoveOrderContext& ctx);
    uint16_t priority(const MoveOrderContext& ctx, const Move& move) const;

private:
    void diversify_root_order(const MoveOrderContext& ctx);

    std::array<Move, MAX_MOVES> moves;
    Move*                       last;
};

inline MoveList::MoveList(const MoveList& other)
    : moves(other.moves),
      last(moves.data() + other.size()) {}

inline MoveList::MoveList(MoveList&& other) noexcept : MoveList(other) {}

inline MoveList& MoveList::operator=(const MoveList& other) {
    if (this == &other)
        return *this;

    moves = other.moves;
    last  = moves.data() + other.size();
    return *this;
}

inline MoveList& MoveList::operator=(MoveList&& other) noexcept {
    return *this = other;
}

inline void MoveList::add(Square from, Square to, MoveType mtype, PieceType prom) {
    assert(size() < moves.size());
    *last++ = Move(from, to, mtype, prom);
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

enum class MovePickerStage : uint8_t {
    PV,
    HASH,
    PROMOTIONS,
    GOOD_TACTICALS,
    KILLERS,
    QUIETS,
    WEAK_TACTICALS,
    DONE,
};

enum class QuiescenceMovePickerStage : uint8_t {
    PROMOTIONS,
    GOOD_TACTICALS,
    KILLERS,
    QUIETS,
    WEAK_TACTICALS,
    DONE,
};

class StagedMovePicker {
public:
    StagedMovePicker(const MoveOrderContext& ctx, MoveList& movelist);

    Move* next();

private:
    Move* remaining_begin();
    Move* remaining_end();
    bool  is_capture(const Move& move) const;
    bool  is_quiet(const Move& move) const;
    void  score_tacticals();
    bool  helper_rotation_enabled(uint16_t score) const;
    Move* consume(Move* move);

    template <typename Predicate, typename Score>
    Move* pick_best(Predicate predicate, Score score);

    MoveOrderContext ctx;
    MoveList&        movelist;
    MovePickerStage  stage{MovePickerStage::PV};
    std::size_t      consumed_count{0};
    bool             tactical_scores_ready{false};
};

class QuiescenceMovePicker {
public:
    QuiescenceMovePicker(const MoveOrderContext& ctx, MoveList& movelist, bool in_check);

    Move* next();

private:
    Move* remaining_begin();
    Move* remaining_end();
    bool  is_capture(const Move& move) const;
    bool  is_quiet(const Move& move) const;
    void  score_tacticals();
    Move* consume(Move* move);

    template <typename Predicate, typename Score>
    Move* pick_best(Predicate predicate, Score score);

    MoveOrderContext          ctx;
    MoveList&                 movelist;
    QuiescenceMovePickerStage stage;
    bool                      in_check;
    std::size_t               consumed_count{0};
    bool                      tactical_scores_ready{false};
};

inline StagedMovePicker::StagedMovePicker(const MoveOrderContext& ctx, MoveList& movelist)
    : ctx(ctx),
      movelist(movelist) {}

inline Move* StagedMovePicker::remaining_begin() {
    return movelist.begin() + consumed_count;
}

inline Move* StagedMovePicker::remaining_end() {
    return movelist.end();
}

inline bool StagedMovePicker::is_capture(const Move& move) const {
    return ctx.board.is_capture(move);
}

inline bool StagedMovePicker::is_quiet(const Move& move) const {
    return move.type() != MOVE_PROM && !is_capture(move);
}

inline void StagedMovePicker::score_tacticals() {
    if (tactical_scores_ready)
        return;

    for (Move* move = remaining_begin(); move != remaining_end(); ++move) {
        if (move->type() == MOVE_PROM || !is_capture(*move))
            continue;

        const int see_score = ctx.board.seeMove(*move);
        move->priority      = (see_score >= 0) ? PRIORITY_CAPTURE + see_score : PRIORITY_WEAK;
    }

    tactical_scores_ready = true;
}

inline bool StagedMovePicker::helper_rotation_enabled(uint16_t score) const {
    return ctx.root && ctx.thread_id != 0 && score < PRIORITY_HASH;
}

inline Move* StagedMovePicker::consume(Move* move) {
    Move* const consumed = remaining_begin();
    std::iter_swap(consumed, move);
    ++consumed_count;
    return consumed;
}

template <typename Predicate, typename Score>
inline Move* StagedMovePicker::pick_best(Predicate predicate, Score score) {
    Move*    best       = nullptr;
    uint16_t best_score = 0;

    for (Move* move = remaining_begin(); move != remaining_end(); ++move) {
        if (!predicate(*move))
            continue;

        const uint16_t move_score = score(*move);
        if (best == nullptr || move_score > best_score) {
            best       = move;
            best_score = move_score;
        }
    }

    if (best == nullptr)
        return nullptr;

    if (helper_rotation_enabled(best_score)) {
        int pick_index = 0;
        int group_size = 0;

        for (Move* move = remaining_begin(); move != remaining_end(); ++move) {
            if (!predicate(*move) || score(*move) != best_score)
                continue;
            ++group_size;
        }

        if (group_size > 1) {
            pick_index = ctx.thread_id % group_size;
            for (Move* move = remaining_begin(); move != remaining_end(); ++move) {
                if (!predicate(*move) || score(*move) != best_score)
                    continue;
                if (pick_index-- == 0) {
                    best = move;
                    break;
                }
            }
        }
    }

    return consume(best);
}

inline Move* StagedMovePicker::next() {
    while (stage != MovePickerStage::DONE) {
        switch (stage) {
        case MovePickerStage::PV:
            stage = MovePickerStage::HASH;
            if (!ctx.pv_move.is_null()) {
                if (Move* move =
                        pick_best([&](const Move& candidate) { return candidate == ctx.pv_move; },
                                  [](const Move&) { return PRIORITY_PV; }))
                    return move;
            }
            break;

        case MovePickerStage::HASH:
            stage = MovePickerStage::PROMOTIONS;
            if (!ctx.tt_move.is_null() && ctx.tt_move != ctx.pv_move) {
                if (Move* move =
                        pick_best([&](const Move& candidate) { return candidate == ctx.tt_move; },
                                  [](const Move&) { return PRIORITY_HASH; }))
                    return move;
            }
            break;

        case MovePickerStage::PROMOTIONS:
            if (Move* move =
                    pick_best([&](const Move& candidate) { return candidate.type() == MOVE_PROM; },
                              [](const Move&) { return PRIORITY_PROM; }))
                return move;
            stage = MovePickerStage::GOOD_TACTICALS;
            break;

        case MovePickerStage::GOOD_TACTICALS:
            score_tacticals();
            if (Move* move = pick_best(
                    [&](const Move& candidate) {
                        return candidate.type() != MOVE_PROM && is_capture(candidate) &&
                               candidate.priority >= PRIORITY_CAPTURE;
                    },
                    [](const Move& candidate) { return candidate.priority; }))
                return move;
            stage = MovePickerStage::KILLERS;
            break;

        case MovePickerStage::KILLERS:
            if (Move* move = pick_best(
                    [&](const Move& candidate) {
                        return is_quiet(candidate) && ctx.killers.is_killer(candidate, ctx.ply);
                    },
                    [](const Move&) { return PRIORITY_KILLER; }))
                return move;
            stage = MovePickerStage::QUIETS;
            break;

        case MovePickerStage::QUIETS:
            if (Move* move = pick_best(
                    [&](const Move& candidate) {
                        return is_quiet(candidate) && !ctx.killers.is_killer(candidate, ctx.ply);
                    },
                    [&](const Move& candidate) {
                        return static_cast<uint16_t>(ctx.history.get(
                            ctx.board.side_to_move(), candidate.from(), candidate.to()));
                    }))
                return move;
            stage = MovePickerStage::WEAK_TACTICALS;
            break;

        case MovePickerStage::WEAK_TACTICALS:
            score_tacticals();
            if (Move* move = pick_best(
                    [&](const Move& candidate) {
                        return candidate.type() != MOVE_PROM && is_capture(candidate) &&
                               candidate.priority < PRIORITY_CAPTURE;
                    },
                    [](const Move& candidate) { return candidate.priority; }))
                return move;
            stage = MovePickerStage::DONE;
            break;

        case MovePickerStage::DONE: break;
        }
    }

    return nullptr;
}

inline QuiescenceMovePicker::QuiescenceMovePicker(const MoveOrderContext& ctx,
                                                  MoveList&               movelist,
                                                  bool                    in_check)
    : ctx(ctx),
      movelist(movelist),
      stage(QuiescenceMovePickerStage::PROMOTIONS),
      in_check(in_check) {}

inline Move* QuiescenceMovePicker::remaining_begin() {
    return movelist.begin() + consumed_count;
}

inline Move* QuiescenceMovePicker::remaining_end() {
    return movelist.end();
}

inline bool QuiescenceMovePicker::is_capture(const Move& move) const {
    return ctx.board.is_capture(move);
}

inline bool QuiescenceMovePicker::is_quiet(const Move& move) const {
    return move.type() != MOVE_PROM && !is_capture(move);
}

inline void QuiescenceMovePicker::score_tacticals() {
    if (tactical_scores_ready)
        return;

    for (Move* move = remaining_begin(); move != remaining_end(); ++move) {
        if (move->type() == MOVE_PROM || !is_capture(*move))
            continue;

        const int see_score = ctx.board.seeMove(*move);
        move->priority      = (see_score >= 0) ? PRIORITY_CAPTURE + see_score : PRIORITY_WEAK;
    }

    tactical_scores_ready = true;
}

inline Move* QuiescenceMovePicker::consume(Move* move) {
    Move* const consumed = remaining_begin();
    std::iter_swap(consumed, move);
    ++consumed_count;
    return consumed;
}

template <typename Predicate, typename Score>
inline Move* QuiescenceMovePicker::pick_best(Predicate predicate, Score score) {
    Move*    best       = nullptr;
    uint16_t best_score = 0;

    for (Move* move = remaining_begin(); move != remaining_end(); ++move) {
        if (!predicate(*move))
            continue;

        const uint16_t move_score = score(*move);
        if (best == nullptr || move_score > best_score) {
            best       = move;
            best_score = move_score;
        }
    }

    return best == nullptr ? nullptr : consume(best);
}

inline Move* QuiescenceMovePicker::next() {
    while (stage != QuiescenceMovePickerStage::DONE) {
        switch (stage) {
        case QuiescenceMovePickerStage::PROMOTIONS:
            if (Move* move =
                    pick_best([&](const Move& candidate) { return candidate.type() == MOVE_PROM; },
                              [](const Move&) { return PRIORITY_PROM; }))
                return move;
            stage = QuiescenceMovePickerStage::GOOD_TACTICALS;
            break;

        case QuiescenceMovePickerStage::GOOD_TACTICALS:
            score_tacticals();
            if (Move* move = pick_best(
                    [&](const Move& candidate) {
                        return candidate.type() != MOVE_PROM && is_capture(candidate) &&
                               candidate.priority >= PRIORITY_CAPTURE;
                    },
                    [](const Move& candidate) { return candidate.priority; }))
                return move;
            stage = in_check ? QuiescenceMovePickerStage::KILLERS
                             : QuiescenceMovePickerStage::WEAK_TACTICALS;
            break;

        case QuiescenceMovePickerStage::KILLERS:
            if (Move* move = pick_best(
                    [&](const Move& candidate) {
                        return is_quiet(candidate) && ctx.killers.is_killer(candidate, ctx.ply);
                    },
                    [](const Move&) { return PRIORITY_KILLER; }))
                return move;
            stage = QuiescenceMovePickerStage::QUIETS;
            break;

        case QuiescenceMovePickerStage::QUIETS:
            if (Move* move = pick_best(
                    [&](const Move& candidate) {
                        return is_quiet(candidate) && !ctx.killers.is_killer(candidate, ctx.ply);
                    },
                    [&](const Move& candidate) {
                        return static_cast<uint16_t>(ctx.history.get(
                            ctx.board.side_to_move(), candidate.from(), candidate.to()));
                    }))
                return move;
            stage = QuiescenceMovePickerStage::WEAK_TACTICALS;
            break;

        case QuiescenceMovePickerStage::WEAK_TACTICALS:
            score_tacticals();
            if (Move* move = pick_best(
                    [&](const Move& candidate) {
                        return candidate.type() != MOVE_PROM && is_capture(candidate) &&
                               candidate.priority < PRIORITY_CAPTURE;
                    },
                    [](const Move& candidate) { return candidate.priority; }))
                return move;
            stage = QuiescenceMovePickerStage::DONE;
            break;

        case QuiescenceMovePickerStage::DONE: break;
        }
    }

    return nullptr;
}
