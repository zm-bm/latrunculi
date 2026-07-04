#include "move_picker.hpp"

#include <cassert>
#include <utility>

#include "board.hpp"
#include "eval.hpp"
#include "move_ordering.hpp"
#include "movegen.hpp"

namespace {

// Coarse score bands, not tuned chess values. In-band terms order moves only
// within their stage; they should not cross into another stage.
constexpr MoveScore GoodCaptureScoreBase = MoveScore{1} << 22;
constexpr MoveScore PromotionScore       = GoodCaptureScoreBase << 6;
constexpr MoveScore WeakCaptureScore     = 0;

// Orders captures within the good-capture band.
constexpr MoveScore CaptureVictimWeight = 7;

static_assert(QuietHistory::MAX_SCORE < GoodCaptureScoreBase);

} // namespace

MovePicker
MovePicker::main_search(const Board& board, const MoveOrdering& ordering, int ply, Move tt_move) {
    return MovePicker(board,
                      ordering.quiets,
                      Mode::MainSearch,
                      tt_move,
                      QuietHints{
                          .killer_1 = ordering.killers.primary(ply),
                          .killer_2 = ordering.killers.secondary(ply),
                          .counter  = ordering.counter_hint(board),
                      });
}

MovePicker MovePicker::qsearch(const Board& board, const MoveOrdering& ordering, Move tt_move) {
    return MovePicker(board, ordering.quiets, Mode::QSearch, tt_move, QuietHints{});
}

MovePicker::MovePicker(const Board&        board,
                       const QuietHistory& quiet_history,
                       Mode                mode,
                       Move                tt_move,
                       QuietHints          quiet_hints)
    : board(board),
      quiet_history(quiet_history),
      mode(mode),
      in_check(board.is_check()),
      side(board.side_to_move()),
      stage(Stage::TT_MOVE) {
    this->tt_move = accepted_tt_hint(tt_move);
    set_quiet_hints(quiet_hints);
}

bool MovePicker::QuietHints::contains(Move move) const {
    return !move.is_null() && (move == killer_1 || move == killer_2 || move == counter);
}

void MovePicker::set_quiet_hints(QuietHints quiet_hints) {
    if (!accepts_quiet_hints())
        return;

    killer_1     = accepted_quiet_hint(quiet_hints.killer_1, QuietHints{});
    killer_2     = accepted_quiet_hint(quiet_hints.killer_2, QuietHints{.killer_1 = killer_1});
    counter_move = accepted_quiet_hint(quiet_hints.counter,
                                       QuietHints{.killer_1 = killer_1, .killer_2 = killer_2});
}

bool MovePicker::accepts_quiet_hints() const {
    return mode == Mode::MainSearch && !in_check;
}

bool MovePicker::matches_tt(Move move) const {
    return move == tt_move;
}

bool MovePicker::matches_quiet_hint(Move move) const {
    return QuietHints{
        .killer_1 = killer_1,
        .killer_2 = killer_2,
        .counter  = counter_move,
    }
        .contains(move);
}

Move MovePicker::accepted_tt_hint(Move move) const {
    if (move.is_null() || !board.is_pseudo_legal(move))
        return NULL_MOVE;

    if (in_check && !board.is_legal_pseudo_move(move))
        return NULL_MOVE;

    if (mode == Mode::QSearch && !in_check && move.type() != MOVE_PROM && !board.is_capture(move))
        return NULL_MOVE;

    return move;
}

Move MovePicker::accepted_quiet_hint(Move move, QuietHints duplicates) const {
    if (move.is_null() || duplicates.contains(move) || matches_tt(move))
        return NULL_MOVE;

    if (move.type() == MOVE_PROM || board.is_capture(move) || !board.is_pseudo_legal(move))
        return NULL_MOVE;

    return move;
}

MoveScore MovePicker::score_quiet(Move move) const {
    return static_cast<MoveScore>(quiet_history.get(side, move.from(), move.to()));
}

MoveScore MovePicker::score_noisy(Move move) const {
    if (move.type() == MOVE_PROM)
        return PromotionScore;

    const int see_score = board.seeMove(move);
    if (see_score < 0)
        return WeakCaptureScore;

    const int victim_value = eval::piece(board.captured_piece_type(move)).mg;
    return GoodCaptureScoreBase + CaptureVictimWeight * victim_value + see_score;
}

template <MovePicker::ScoreKind Kind>
MoveScore MovePicker::score_move(Move move) const {
    if constexpr (Kind == ScoreKind::Quiet)
        return score_quiet(move);

    if constexpr (Kind == ScoreKind::Noisy)
        return score_noisy(move);

    if (move.type() == MOVE_PROM || board.is_capture(move))
        return score_noisy(move);

    return score_quiet(move);
}

template <MovePicker::ScoreKind Kind>
ScoredMove* MovePicker::score_moves(const MoveList& list, ScoredMove* out) {
    ScoredMove*       cur   = out;
    ScoredMove* const limit = moves.data() + moves.size();

    for (Move move : list) {
        assert(cur < limit);
        cur->move  = move;
        cur->score = score_move<Kind>(move);
        ++cur;
    }

    return cur;
}

template <MovePicker::PickKind Kind>
bool MovePicker::is_pickable(const ScoredMove& scored_move) const {
    const Move      move  = scored_move.move;
    const MoveScore score = scored_move.score;

    if (matches_tt(move))
        return false;

    if constexpr (Kind == PickKind::Evasion) {
        return true;
    } else if constexpr (Kind == PickKind::GoodNoisy) {
        return move.type() == MOVE_PROM || score >= GoodCaptureScoreBase;
    } else if constexpr (Kind == PickKind::Quiet) {
        return !matches_quiet_hint(move);
    } else {
        return move.type() != MOVE_PROM && score < GoodCaptureScoreBase;
    }
}

template <MovePicker::PickKind Kind>
Move MovePicker::pick(ScoredBand& band) {
    ScoredMove* best       = nullptr;
    MoveScore   best_score = 0;

    for (ScoredMove* candidate = band.cur; candidate != band.end; ++candidate) {
        if (!is_pickable<Kind>(*candidate))
            continue;

        const MoveScore score = candidate->score;
        if (best == nullptr || score > best_score) {
            best       = candidate;
            best_score = score;
        }
    }

    if (best == nullptr)
        return NULL_MOVE;

    std::swap(*band.cur, *best);
    Move move = band.cur->move;
    ++band.cur;
    return move;
}

void MovePicker::skip_quiet_moves() {
    if (mode != Mode::MainSearch || in_check)
        return;

    skip_quiets = true;
    if (stage == Stage::KILLER_1 || stage == Stage::KILLER_2 || stage == Stage::COUNTERMOVE ||
        stage == Stage::LOAD_QUIET || stage == Stage::PICK_QUIET)
        stage = Stage::PICK_BAD_NOISY;
}

Move MovePicker::next() {
    while (stage != Stage::DONE) {
        switch (stage) {
        case Stage::TT_MOVE:
            stage = in_check ? Stage::LOAD_EVASIONS : Stage::LOAD_NOISY;
            if (!tt_move.is_null())
                return tt_move;
            break;

        case Stage::LOAD_EVASIONS: {
            generated.cur                = moves.data();
            const MoveList evasion_moves = movegen::generate_evasions(board);
            generated.end = score_moves<ScoreKind::Evasion>(evasion_moves, generated.cur);
            quiet.cur     = generated.end;
            quiet.end     = generated.end;
            stage         = Stage::PICK_EVASION;
            [[fallthrough]];
        }

        case Stage::PICK_EVASION: {
            Move move = pick<PickKind::Evasion>(generated);
            if (!move.is_null())
                return move;
            stage = Stage::DONE;
            break;
        }

        case Stage::LOAD_NOISY: {
            generated.cur              = moves.data();
            const MoveList noisy_moves = movegen::generate_noisy(board);
            generated.end              = score_moves<ScoreKind::Noisy>(noisy_moves, generated.cur);
            quiet.cur                  = generated.end;
            quiet.end                  = generated.end;
            stage                      = Stage::PICK_GOOD_NOISY;
            [[fallthrough]];
        }

        case Stage::PICK_GOOD_NOISY: {
            Move move = pick<PickKind::GoodNoisy>(generated);
            if (!move.is_null())
                return move;

            if (mode == Mode::QSearch)
                stage = Stage::DONE;
            else
                stage = skip_quiets ? Stage::PICK_BAD_NOISY : Stage::KILLER_1;
            break;
        }

        case Stage::KILLER_1:
            stage = Stage::KILLER_2;
            if (!killer_1.is_null())
                return killer_1;
            break;

        case Stage::KILLER_2:
            stage = Stage::COUNTERMOVE;
            if (!killer_2.is_null())
                return killer_2;
            break;

        case Stage::COUNTERMOVE:
            stage = Stage::LOAD_QUIET;
            if (!counter_move.is_null())
                return counter_move;
            break;

        case Stage::LOAD_QUIET: {
            if (skip_quiets) {
                stage = Stage::PICK_BAD_NOISY;
                break;
            }
            assert(generated.end != nullptr);
            quiet.cur                  = generated.end;
            const MoveList quiet_moves = movegen::generate_quiet(board);
            quiet.end                  = score_moves<ScoreKind::Quiet>(quiet_moves, quiet.cur);
            stage                      = Stage::PICK_QUIET;
            [[fallthrough]];
        }

        case Stage::PICK_QUIET: {
            Move move = pick<PickKind::Quiet>(quiet);
            if (!move.is_null())
                return move;
            stage = Stage::PICK_BAD_NOISY;
            break;
        }

        case Stage::PICK_BAD_NOISY: {
            Move move = pick<PickKind::BadNoisy>(generated);
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
