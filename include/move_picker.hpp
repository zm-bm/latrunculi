#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "board.hpp"
#include "defs.hpp"
#include "history.hpp"
#include "killers.hpp"
#include "move_list.hpp"

struct MovePickerContext {
    Board&        board;
    KillerMoves&  killers;
    HistoryTable& history;
    int           ply;
    Move          pv_move;
    Move          tt_move;
    bool          root      = false;
    int           thread_id = 0;
};

enum class MovePickerMode : uint8_t {
    MainSearch,
    QSearchCaptures,
    QSearchEvasions,
};

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

class MovePicker {
public:
    MovePicker(const MovePickerContext& ctx, MoveList& movelist, MovePickerMode mode);

    Move*    next();
    uint16_t last_score() const { return last_picked_score; }

private:
    MovePickerStage initial_stage() const;
    Move*           remaining_begin();
    Move*           remaining_end();
    std::size_t     index_of(const Move* move) const;
    uint16_t        score_at(const Move* move) const;
    uint16_t&       score_at(Move* move);
    bool            is_capture(const Move& move) const;
    bool            is_quiet(const Move& move) const;
    bool            includes_quiet_stages() const;
    void            score_tacticals();
    bool            helper_rotation_enabled(uint16_t score) const;
    Move*           consume(Move* move, uint16_t score);

    template <typename Predicate, typename Score>
    Move* pick_best(Predicate predicate, Score score);

    MovePickerContext               ctx;
    MoveList&                       movelist;
    MovePickerMode                  mode;
    std::array<uint16_t, MAX_MOVES> scores{};
    MovePickerStage                 stage;
    std::size_t                     consumed_count{0};
    bool                            tactical_scores_ready{false};
    uint16_t                        last_picked_score{0};
};

inline MovePicker::MovePicker(const MovePickerContext& ctx, MoveList& movelist, MovePickerMode mode)
    : ctx(ctx),
      movelist(movelist),
      mode(mode),
      stage(initial_stage()) {}

inline MovePickerStage MovePicker::initial_stage() const {
    return mode == MovePickerMode::MainSearch ? MovePickerStage::PV : MovePickerStage::PROMOTIONS;
}

inline Move* MovePicker::remaining_begin() {
    return movelist.begin() + consumed_count;
}

inline Move* MovePicker::remaining_end() {
    return movelist.end();
}

inline std::size_t MovePicker::index_of(const Move* move) const {
    const auto index = static_cast<std::size_t>(move - movelist.begin());
    assert(index < movelist.size());
    return index;
}

inline uint16_t MovePicker::score_at(const Move* move) const {
    return scores[index_of(move)];
}

inline uint16_t& MovePicker::score_at(Move* move) {
    return scores[index_of(move)];
}

inline bool MovePicker::is_capture(const Move& move) const {
    return ctx.board.is_capture(move);
}

inline bool MovePicker::is_quiet(const Move& move) const {
    return move.type() != MOVE_PROM && !is_capture(move);
}

inline bool MovePicker::includes_quiet_stages() const {
    return mode != MovePickerMode::QSearchCaptures;
}

inline void MovePicker::score_tacticals() {
    if (tactical_scores_ready)
        return;

    for (Move* move = remaining_begin(); move != remaining_end(); ++move) {
        if (move->type() == MOVE_PROM || !is_capture(*move))
            continue;

        const int see_score = ctx.board.seeMove(*move);
        score_at(move)      = (see_score >= 0) ? PRIORITY_CAPTURE + see_score : PRIORITY_WEAK;
    }

    tactical_scores_ready = true;
}

inline bool MovePicker::helper_rotation_enabled(uint16_t score) const {
    return mode == MovePickerMode::MainSearch && ctx.root && ctx.thread_id != 0 &&
           score < PRIORITY_HASH;
}

inline Move* MovePicker::consume(Move* move, uint16_t score) {
    Move* const consumed = remaining_begin();
    std::swap(scores[index_of(consumed)], scores[index_of(move)]);
    std::iter_swap(consumed, move);
    ++consumed_count;
    last_picked_score = score;
    return consumed;
}

template <typename Predicate, typename Score>
inline Move* MovePicker::pick_best(Predicate predicate, Score score) {
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

    return consume(best, best_score);
}

inline Move* MovePicker::next() {
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
                               score_at(&candidate) >= PRIORITY_CAPTURE;
                    },
                    [&](const Move& candidate) { return score_at(&candidate); }))
                return move;
            stage = includes_quiet_stages() ? MovePickerStage::KILLERS
                                            : MovePickerStage::WEAK_TACTICALS;
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
                               score_at(&candidate) < PRIORITY_CAPTURE;
                    },
                    [&](const Move& candidate) { return score_at(&candidate); }))
                return move;
            stage = MovePickerStage::DONE;
            break;

        case MovePickerStage::DONE: break;
        }
    }

    return nullptr;
}
