#include "move_picker.hpp"

#include <cassert>
#include <utility>

#include "board.hpp"
#include "eval.hpp"
#include "history.hpp"
#include "killers.hpp"
#include "movegen_impl.hpp"

namespace {

constexpr MoveScore CAPTURE_VICTIM_WEIGHT = 7;

class MovePickerOutput {
public:
    MovePickerOutput(ScoredMove* out, ScoredMove* limit) : out(out), limit(limit) {}

    void add(Move move) {
        assert(out < limit);
        out->move = move;
        ++out;
    }

    void
    add(Square from, Square to, MoveType move_type = BASIC_MOVE, PieceType promotion = KNIGHT) {
        add(Move(from, to, move_type, promotion));
    }

    ScoredMove* end() const { return out; }

private:
    ScoredMove* out;
    ScoredMove* limit;
};

MoveScore capture_score(const Board& board, Move move) {
    const int see_score = board.seeMove(move);
    if (see_score < 0)
        return PRIORITY_WEAK;

    return PRIORITY_CAPTURE +
           CAPTURE_VICTIM_WEIGHT * eval::piece(board.captured_piece_type(move)).mg + see_score;
}

} // namespace

MovePicker::MovePicker(const MovePickerContext& ctx, MovePickerMode mode)
    : ctx(ctx),
      mode(mode),
      in_check(ctx.board.is_check()),
      valid_pv_move(is_valid_hint(ctx.pv_move)),
      valid_tt_move(ctx.tt_move != ctx.pv_move && is_valid_hint(ctx.tt_move)),
      stage(initial_stage()) {}

MovePicker::Stage MovePicker::initial_stage() const {
    if (mode == MovePickerMode::QSearch)
        return in_check ? Stage::GENERATE_EVASIONS : Stage::GENERATE_NOISY;

    return Stage::PV;
}

bool MovePicker::includes_quiet_stages() const {
    return mode == MovePickerMode::MainSearch && !in_check && !skip_quiets;
}

bool MovePicker::includes_killer_stages() const {
    return mode == MovePickerMode::MainSearch && !in_check;
}

bool MovePicker::is_capture(const Move& move) const {
    return ctx.board.is_capture(move);
}

bool MovePicker::is_quiet(const Move& move) const {
    return move.type() != MOVE_PROM && !is_capture(move);
}

bool MovePicker::is_valid_hint(Move move) const {
    if (move.is_null() || !ctx.board.is_pseudo_legal(move))
        return false;

    return !in_check || ctx.board.is_legal_pseudo_move(move);
}

bool MovePicker::is_pv_or_tt_duplicate(const Move& move) const {
    return (valid_pv_move && move == ctx.pv_move) || (valid_tt_move && move == ctx.tt_move);
}

bool MovePicker::is_quiet_hint_duplicate(const Move& move) const {
    return is_pv_or_tt_duplicate(move) || move == killer_1 || move == killer_2;
}

bool MovePicker::is_returnable_killer(Move move, Move previous_killer) const {
    return !move.is_null() && move != previous_killer && is_quiet(move) &&
           !is_pv_or_tt_duplicate(move) && ctx.board.is_pseudo_legal(move);
}

void MovePicker::score_noisy_moves(ScoredMove* begin, ScoredMove* end) {
    for (ScoredMove* candidate = begin; candidate != end; ++candidate) {
        if (candidate->move.type() == MOVE_PROM)
            candidate->score = PRIORITY_PROM;
        else
            candidate->score = capture_score(ctx.board, candidate->move);
    }
}

void MovePicker::score_quiet_moves(ScoredMove* begin, ScoredMove* end) {
    const Color side = ctx.board.side_to_move();
    for (ScoredMove* candidate = begin; candidate != end; ++candidate)
        candidate->score = static_cast<MoveScore>(
            ctx.history.get(side, candidate->move.from(), candidate->move.to()));
}

void MovePicker::score_evasion_moves(ScoredMove* begin, ScoredMove* end) {
    const Color side = ctx.board.side_to_move();
    for (ScoredMove* candidate = begin; candidate != end; ++candidate) {
        if (candidate->move.type() == MOVE_PROM)
            candidate->score = PRIORITY_PROM;
        else if (is_capture(candidate->move))
            candidate->score = capture_score(ctx.board, candidate->move);
        else
            candidate->score = static_cast<MoveScore>(
                ctx.history.get(side, candidate->move.from(), candidate->move.to()));
    }
}

Move MovePicker::consume(ScoredMove*&   cursor,
                         ScoredMove*    picked,
                         MoveScore      score,
                         MovePickSource source) {
    std::swap(*cursor, *picked);
    Move move = cursor->move;
    ++cursor;

    last_picked_score  = score;
    last_picked_source = source;
    return move;
}

void MovePicker::generate_noisy_moves() {
    if (noisy_ready)
        return;

    MovePickerOutput output(moves_begin(), moves_limit());
    movegen::generate_into<movegen::MoveGenType::Noisy>(ctx.board, output);

    primary_cur   = moves_begin();
    end_primary   = output.end();
    end_generated = end_primary;
    quiet_cur     = end_primary;

    score_noisy_moves(primary_cur, end_primary);
    noisy_ready = true;
}

void MovePicker::generate_quiet_moves() {
    if (quiet_ready)
        return;

    assert(noisy_ready);
    MovePickerOutput output(end_primary, moves_limit());
    movegen::generate_into<movegen::MoveGenType::Quiet>(ctx.board, output);

    quiet_cur     = end_primary;
    end_generated = output.end();

    score_quiet_moves(quiet_cur, end_generated);
    quiet_ready = true;
}

void MovePicker::generate_evasion_moves() {
    if (evasion_ready)
        return;

    MovePickerOutput output(moves_begin(), moves_limit());
    movegen::generate_into<movegen::MoveGenType::Evasions>(ctx.board, output);

    primary_cur   = moves_begin();
    end_primary   = output.end();
    end_generated = end_primary;
    quiet_cur     = end_primary;

    score_evasion_moves(primary_cur, end_primary);
    evasion_ready = true;
}

template <typename Predicate>
Move MovePicker::pick_best(ScoredMove*&   cursor,
                           ScoredMove*    end,
                           Predicate      predicate,
                           MovePickSource source) {
    ScoredMove* best       = nullptr;
    MoveScore   best_score = 0;

    for (ScoredMove* candidate = cursor; candidate != end; ++candidate) {
        if (!predicate(candidate->move, candidate->score))
            continue;

        if (best == nullptr || candidate->score > best_score) {
            best       = candidate;
            best_score = candidate->score;
        }
    }

    if (best == nullptr)
        return NULL_MOVE;

    return consume(cursor, best, best_score, source);
}

void MovePicker::skip_quiet_moves() {
    if (mode != MovePickerMode::MainSearch || in_check)
        return;

    skip_quiets = true;
    if (stage == Stage::KILLER_1 || stage == Stage::KILLER_2 || stage == Stage::GENERATE_QUIET ||
        stage == Stage::QUIET)
        stage = Stage::BAD_NOISY;
}

Move MovePicker::next() {
    while (stage != Stage::DONE) {
        switch (stage) {
        case Stage::PV:
            stage = Stage::HASH;
            if (valid_pv_move) {
                last_picked_score  = PRIORITY_PV;
                last_picked_source = MovePickSource::PV;
                return ctx.pv_move;
            }
            break;

        case Stage::HASH:
            stage = in_check ? Stage::GENERATE_EVASIONS : Stage::GENERATE_NOISY;
            if (valid_tt_move) {
                last_picked_score  = PRIORITY_HASH;
                last_picked_source = MovePickSource::Hash;
                return ctx.tt_move;
            }
            break;

        case Stage::GENERATE_EVASIONS:
            generate_evasion_moves();
            stage = Stage::EVASION;
            break;

        case Stage::EVASION: {
            Move move = pick_best(
                primary_cur,
                end_primary,
                [&](const Move& candidate, MoveScore) { return !is_pv_or_tt_duplicate(candidate); },
                MovePickSource::Evasion);
            if (!move.is_null())
                return move;
            stage = Stage::DONE;
            break;
        }

        case Stage::GENERATE_NOISY:
            generate_noisy_moves();
            stage = Stage::GOOD_NOISY;
            break;

        case Stage::GOOD_NOISY: {
            Move move = pick_best(
                primary_cur,
                end_primary,
                [&](const Move& candidate, MoveScore score) {
                    return !is_pv_or_tt_duplicate(candidate) &&
                           (candidate.type() == MOVE_PROM || score >= PRIORITY_CAPTURE);
                },
                MovePickSource::GoodNoisy);
            if (!move.is_null())
                return move;

            if (mode == MovePickerMode::QSearch) {
                stage = Stage::DONE;
            } else if (includes_killer_stages()) {
                stage = Stage::KILLER_1;
            } else {
                stage = Stage::DONE;
            }
            break;
        }

        case Stage::KILLER_1:
            stage    = Stage::KILLER_2;
            killer_1 = ctx.killers.primary(ctx.ply);
            if (is_returnable_killer(killer_1, NULL_MOVE)) {
                last_picked_score  = PRIORITY_KILLER;
                last_picked_source = MovePickSource::Killer;
                return killer_1;
            }
            break;

        case Stage::KILLER_2:
            stage    = includes_quiet_stages() ? Stage::GENERATE_QUIET : Stage::BAD_NOISY;
            killer_2 = ctx.killers.secondary(ctx.ply);
            if (is_returnable_killer(killer_2, killer_1)) {
                last_picked_score  = PRIORITY_KILLER;
                last_picked_source = MovePickSource::Killer;
                return killer_2;
            }
            break;

        case Stage::GENERATE_QUIET:
            if (skip_quiets) {
                stage = Stage::BAD_NOISY;
                break;
            }
            generate_quiet_moves();
            stage = Stage::QUIET;
            break;

        case Stage::QUIET: {
            Move move = pick_best(
                quiet_cur,
                end_generated,
                [&](const Move& candidate, MoveScore) {
                    return !is_quiet_hint_duplicate(candidate);
                },
                MovePickSource::Quiet);
            if (!move.is_null())
                return move;
            stage = Stage::BAD_NOISY;
            break;
        }

        case Stage::BAD_NOISY: {
            Move move = pick_best(
                primary_cur,
                end_primary,
                [&](const Move& candidate, MoveScore score) {
                    return !is_pv_or_tt_duplicate(candidate) && candidate.type() != MOVE_PROM &&
                           score < PRIORITY_CAPTURE;
                },
                MovePickSource::BadNoisy);
            if (!move.is_null())
                return move;
            stage = Stage::DONE;
            break;
        }

        case Stage::DONE: break;
        }
    }

    return NULL_MOVE;
}
