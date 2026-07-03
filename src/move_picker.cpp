#include "move_picker.hpp"

#include <cassert>
#include <utility>

#include "board.hpp"
#include "eval.hpp"
#include "history.hpp"
#include "killers.hpp"
#include "movegen.hpp"

namespace {

// Coarse score bands, not tuned chess values. In-band terms order moves only
// within their stage; they should not cross into another stage.
constexpr MoveScore GoodCaptureScoreBase = MoveScore{1} << 22;
constexpr MoveScore PromotionScore       = GoodCaptureScoreBase << 6;
constexpr MoveScore WeakCaptureScore     = 0;

// Orders SEE-safe captures within the good-capture band.
constexpr MoveScore CaptureVictimWeight = 7;

static_assert(HistoryTable::MAX_SCORE < GoodCaptureScoreBase);

MoveScore capture_score(const Board& board, Move move) {
    const int see_score = board.seeMove(move);
    if (see_score < 0)
        return WeakCaptureScore;

    const int victim_value = eval::piece(board.captured_piece_type(move)).mg;
    return GoodCaptureScoreBase + CaptureVictimWeight * victim_value + see_score;
}

} // namespace

MovePicker MovePicker::main_search(const Board&        board,
                                   const KillerMoves&  killers,
                                   const HistoryTable& history,
                                   int                 ply,
                                   Move                tt_move) {
    return MovePicker(
        board, history, Mode::MainSearch, tt_move, killers.primary(ply), killers.secondary(ply));
}

MovePicker MovePicker::qsearch(const Board& board, const HistoryTable& history, Move tt_move) {
    return MovePicker(board, history, Mode::QSearch, tt_move, NULL_MOVE, NULL_MOVE);
}

MovePicker::MovePicker(const Board&        board,
                       const HistoryTable& history,
                       Mode                mode,
                       Move                tt_move,
                       Move                killer_1,
                       Move                killer_2)
    : board(board),
      history(history),
      mode(mode),
      in_check(board.is_check()),
      side(board.side_to_move()),
      stage(Stage::TT_MOVE) {
    this->tt_move = is_valid_tt_hint(tt_move) ? tt_move : NULL_MOVE;

    if (mode == Mode::MainSearch && !in_check) {
        this->killer_1 = is_returnable_killer(killer_1, NULL_MOVE) ? killer_1 : NULL_MOVE;
        this->killer_2 = is_returnable_killer(killer_2, this->killer_1) ? killer_2 : NULL_MOVE;
    }
}

bool MovePicker::is_tt_duplicate(Move move) const {
    return move == tt_move;
}

bool MovePicker::is_quiet_hint_duplicate(Move move) const {
    return is_tt_duplicate(move) || move == killer_1 || move == killer_2;
}

bool MovePicker::is_valid_hint(Move move) const {
    if (move.is_null() || !board.is_pseudo_legal(move))
        return false;

    return !in_check || board.is_legal_pseudo_move(move);
}

bool MovePicker::is_valid_tt_hint(Move move) const {
    if (!is_valid_hint(move))
        return false;

    if (mode != Mode::QSearch || in_check)
        return true;

    return move.type() == MOVE_PROM || board.is_capture(move);
}

bool MovePicker::is_returnable_killer(Move move, Move previous_killer) const {
    return !move.is_null() && move != previous_killer && move.type() != MOVE_PROM &&
           !board.is_capture(move) && !is_tt_duplicate(move) && board.is_pseudo_legal(move);
}

template <MovePicker::ScoreKind Kind>
MoveScore MovePicker::score_move(Move move) const {
    if constexpr (Kind == ScoreKind::Noisy) {
        return move.type() == MOVE_PROM ? PromotionScore : capture_score(board, move);
    } else if constexpr (Kind == ScoreKind::Quiet) {
        return static_cast<MoveScore>(history.get(side, move.from(), move.to()));
    } else {
        if (move.type() == MOVE_PROM)
            return PromotionScore;
        if (board.is_capture(move))
            return capture_score(board, move);
        return static_cast<MoveScore>(history.get(side, move.from(), move.to()));
    }
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

    if constexpr (Kind == PickKind::Evasion) {
        return !is_tt_duplicate(move);
    } else if constexpr (Kind == PickKind::GoodNoisy) {
        return !is_tt_duplicate(move) &&
               (move.type() == MOVE_PROM || score >= GoodCaptureScoreBase);
    } else if constexpr (Kind == PickKind::Quiet) {
        return !is_quiet_hint_duplicate(move);
    } else {
        return !is_tt_duplicate(move) && move.type() != MOVE_PROM && score < GoodCaptureScoreBase;
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
    if (stage == Stage::KILLER_1 || stage == Stage::KILLER_2 || stage == Stage::LOAD_QUIET ||
        stage == Stage::PICK_QUIET)
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
            stage = Stage::LOAD_QUIET;
            if (!killer_2.is_null())
                return killer_2;
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
