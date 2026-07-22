#include "search/move_picker.hpp"

#include <cassert>
#include <utility>

#include "board/board.hpp"
#include "eval/eval.hpp"
#include "movegen/movegen.hpp"
#include "search/move_ordering.hpp"

namespace {

// Coarse score bands, not tuned chess values. In-band terms order moves only
// within their stage; they should not cross into another stage.
constexpr int GoodCaptureScoreBase = 1 << 22;
constexpr int PromotionScore       = GoodCaptureScoreBase << 6;
constexpr int WeakCaptureScore     = 0;

// Orders captures within the good-capture band.
constexpr int CaptureVictimWeight = 7;

static_assert(QuietHistory::max_score + ContinuationHistory::max_score < GoodCaptureScoreBase);

} // namespace

namespace move_picker {

Picker main_search(const Board&                 board,
                   const MoveOrdering&          ordering,
                   const MoveOrdering::Context& context,
                   int                          ply,
                   Move                         tt_move) {
    const Picker::QuietHintCandidates quiet_hint_candidates{
        ordering.killers.primary(ply),
        ordering.killers.secondary(ply),
        ordering.counter_hint(context),
    };

    return Picker(Mode::MainSearch, board, ordering, context, tt_move, quiet_hint_candidates);
}

Picker qsearch(const Board&                 board,
               const MoveOrdering&          ordering,
               const MoveOrdering::Context& context,
               Move                         tt_move) {
    return Picker(Mode::QSearch, board, ordering, context, tt_move);
}

Picker::Picker(Mode                         mode,
               const Board&                 board,
               const MoveOrdering&          ordering,
               const MoveOrdering::Context& context,
               Move                         tt,
               QuietHintCandidates          quiet_hint_candidates)
    : board(board),
      ordering(ordering),
      context(context),
      mode(mode),
      in_check(board.is_check()),
      stage(Stage::TT_MOVE) {
    tt_move = accepted_tt_hint(tt);

    if (mode == Mode::MainSearch && !in_check) {
        for (Move hint : quiet_hint_candidates)
            add_quiet_hint(hint);
    }
}

void Picker::add_quiet_hint(Move move) {
    Move accepted = accepted_quiet_hint(move);
    if (accepted.is_null())
        return;

    assert(quiet_hint_count < QuietHintCapacity);
    quiet_hints[quiet_hint_count] = accepted;
    ++quiet_hint_count;
}

bool Picker::matches_tt(Move move) const {
    return move == tt_move;
}

bool Picker::matches_quiet_hint(Move move) const {
    if (move.is_null())
        return false;

    for (int i = 0; i < quiet_hint_count; ++i) {
        if (quiet_hints[i] == move)
            return true;
    }

    return false;
}

Move Picker::accepted_tt_hint(Move move) const {
    if (move.is_null() || !board.is_pseudo_legal(move))
        return NULL_MOVE;

    if (in_check && !board.is_legal_pseudo_move(move))
        return NULL_MOVE;

    if (mode == Mode::QSearch && !in_check && move.type() != MOVE_PROM && !board.is_capture(move))
        return NULL_MOVE;

    return move;
}

Move Picker::accepted_quiet_hint(Move move) const {
    if (move.is_null() || matches_quiet_hint(move) || matches_tt(move))
        return NULL_MOVE;

    if (move.type() == MOVE_PROM || board.is_capture(move) || !board.is_pseudo_legal(move))
        return NULL_MOVE;

    return move;
}

Move Picker::next_quiet_hint() {
    if (quiet_hint_next == quiet_hint_count)
        return NULL_MOVE;

    Move move = quiet_hints[quiet_hint_next];
    ++quiet_hint_next;
    return move;
}

int Picker::score_noisy(Move move) const {
    if (move.type() == MOVE_PROM)
        return PromotionScore;

    const int see_score = board.see(move);
    if (see_score < 0)
        return WeakCaptureScore;

    const int victim_value = eval::piece(board.captured_piece_type(move)).mg;
    return GoodCaptureScoreBase + CaptureVictimWeight * victim_value + see_score;
}

template <ScorePolicy Policy>
int Picker::score_move(Move move) const {
    if constexpr (Policy == ScorePolicy::Quiet)
        return ordering.quiet_score(context, board, move, true);

    if constexpr (Policy == ScorePolicy::Noisy)
        return score_noisy(move);

    if (move.type() == MOVE_PROM || board.is_capture(move))
        return score_noisy(move);

    return ordering.quiet_score(context, board, move, false);
}

template <ScorePolicy Policy>
Candidate* Picker::score_moves(const MoveList& list, Candidate* out) {
    Candidate*       cur   = out;
    Candidate* const limit = moves.data() + moves.size();

    for (Move move : list) {
        assert(cur < limit);
        cur->move  = move;
        cur->score = score_move<Policy>(move);
        ++cur;
    }

    return cur;
}

template <PickPolicy Policy>
bool Picker::is_pickable(const Candidate& candidate) const {
    const Move move  = candidate.move;
    const int  score = candidate.score;

    if (matches_tt(move))
        return false;

    if constexpr (Policy == PickPolicy::Evasion) {
        return true;
    } else if constexpr (Policy == PickPolicy::GoodNoisy) {
        return move.type() == MOVE_PROM || score >= GoodCaptureScoreBase;
    } else if constexpr (Policy == PickPolicy::Quiet) {
        return !matches_quiet_hint(move);
    } else {
        return move.type() != MOVE_PROM && score < GoodCaptureScoreBase;
    }
}

template <PickPolicy Policy>
Move Picker::pick(CandidateRange& range) {
    Candidate* best       = nullptr;
    int        best_score = 0;

    for (Candidate* candidate = range.next; candidate != range.end; ++candidate) {
        if (!is_pickable<Policy>(*candidate))
            continue;

        const int score = candidate->score;
        if (best == nullptr || score > best_score) {
            best       = candidate;
            best_score = score;
        }
    }

    if (best == nullptr)
        return NULL_MOVE;

    std::swap(*range.next, *best);
    Move move = range.next->move;
    ++range.next;
    return move;
}

void Picker::skip_quiet_moves() {
    if (mode != Mode::MainSearch || in_check)
        return;

    skip_quiets = true;

    switch (stage) {
    case Stage::PICK_QUIET_HINT:
    case Stage::LOAD_QUIET:
    case Stage::PICK_QUIET:      stage = Stage::PICK_BAD_NOISY; break;

    default: break;
    }
}

Move Picker::next() {
    while (stage != Stage::DONE) {
        switch (stage) {
        case Stage::TT_MOVE:
            stage = in_check ? Stage::LOAD_EVASIONS : Stage::LOAD_NOISY;
            if (!tt_move.is_null())
                return tt_move;
            break;

        case Stage::LOAD_EVASIONS: {
            primary.next                 = moves.data();
            const MoveList evasion_moves = movegen::generate_evasions(board);
            primary.end = score_moves<ScorePolicy::Evasion>(evasion_moves, primary.next);
            quiets.next = primary.end;
            quiets.end  = primary.end;
            stage       = Stage::PICK_EVASION;
            [[fallthrough]];
        }

        case Stage::PICK_EVASION: {
            Move move = pick<PickPolicy::Evasion>(primary);
            if (!move.is_null())
                return move;
            stage = Stage::DONE;
            break;
        }

        case Stage::LOAD_NOISY: {
            primary.next               = moves.data();
            const MoveList noisy_moves = movegen::generate_noisy(board);
            primary.end                = score_moves<ScorePolicy::Noisy>(noisy_moves, primary.next);
            quiets.next                = primary.end;
            quiets.end                 = primary.end;
            stage                      = Stage::PICK_GOOD_NOISY;
            [[fallthrough]];
        }

        case Stage::PICK_GOOD_NOISY: {
            Move move = pick<PickPolicy::GoodNoisy>(primary);
            if (!move.is_null())
                return move;

            if (mode == Mode::QSearch)
                stage = Stage::DONE;
            else
                stage = skip_quiets ? Stage::PICK_BAD_NOISY : Stage::PICK_QUIET_HINT;
            break;
        }

        case Stage::PICK_QUIET_HINT: {
            Move move = next_quiet_hint();
            if (!move.is_null())
                return move;
            stage = Stage::LOAD_QUIET;
            break;
        }

        case Stage::LOAD_QUIET: {
            if (skip_quiets) {
                stage = Stage::PICK_BAD_NOISY;
                break;
            }
            assert(primary.end != nullptr);
            quiets.next                = primary.end;
            const MoveList quiet_moves = movegen::generate_quiet(board);
            quiets.end                 = score_moves<ScorePolicy::Quiet>(quiet_moves, quiets.next);
            stage                      = Stage::PICK_QUIET;
            [[fallthrough]];
        }

        case Stage::PICK_QUIET: {
            Move move = pick<PickPolicy::Quiet>(quiets);
            if (!move.is_null())
                return move;
            stage = Stage::PICK_BAD_NOISY;
            break;
        }

        case Stage::PICK_BAD_NOISY: {
            Move move = pick<PickPolicy::BadNoisy>(primary);
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

} // namespace move_picker
