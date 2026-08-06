#include "search/ordering/picker.hpp"

#include <cassert>
#include <utility>

#include "board/board.hpp"
#include "eval/eval.hpp"
#include "movegen/generator.hpp"

namespace search::ordering {

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

Picker main_search(const Board&          board,
                   const State&          ordering,
                   const State::Context& context,
                   int                   ply,
                   Move                  tt_move) {
    const Picker::QuietHintCandidates quiet_hint_candidates{
        ordering.killers.primary(ply),
        ordering.killers.secondary(ply),
        ordering.counter_hint(context),
    };

    return Picker(
        Picker::Mode::MainSearch, board, ordering, context, tt_move, quiet_hint_candidates);
}

Picker qsearch(const Board& board, const State& ordering, Move tt_move) {
    const State::Context context{.side = board.side_to_move()};
    return Picker(Picker::Mode::QSearch, board, ordering, context, tt_move);
}

Picker::Picker(Mode                  mode,
               const Board&          board,
               const State&          ordering,
               const State::Context& context,
               Move                  tt,
               QuietHintCandidates   quiet_hint_candidates)
    : board(board),
      ordering(ordering),
      context(context),
      mode(mode),
      in_check(board.is_check()),
      stage(Stage::TtMove) {
    tt_move = validate_tt_hint(tt);

    if (mode == Mode::MainSearch && !in_check) {
        for (Move hint : quiet_hint_candidates)
            add_quiet_hint(hint);
    }
}

void Picker::add_quiet_hint(Move move) {
    Move accepted = validate_quiet_hint(move);
    if (accepted.is_null())
        return;

    assert(quiet_hint_count < QuietHintCapacity);
    quiet_hints[quiet_hint_count] = accepted;
    ++quiet_hint_count;
}

bool Picker::is_tt_move(Move move) const {
    return move == tt_move;
}

bool Picker::is_quiet_hint(Move move) const {
    if (move.is_null())
        return false;

    for (int i = 0; i < quiet_hint_count; ++i) {
        if (quiet_hints[i] == move)
            return true;
    }

    return false;
}

Move Picker::validate_tt_hint(Move move) const {
    if (move.is_null() || !board.is_pseudo_legal(move))
        return NULL_MOVE;

    if (in_check && !board.is_legal_pseudo_move(move))
        return NULL_MOVE;

    if (mode == Mode::QSearch && !in_check && move.type() != MOVE_PROM && !board.is_capture(move))
        return NULL_MOVE;

    return move;
}

Move Picker::validate_quiet_hint(Move move) const {
    if (move.is_null() || is_quiet_hint(move) || is_tt_move(move))
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

template <Picker::ScorePolicy Policy>
int Picker::score_move(Move move) const {
    if constexpr (Policy == ScorePolicy::Quiet)
        return ordering.quiet_score(context, board, move, true);

    if constexpr (Policy == ScorePolicy::Noisy)
        return score_noisy(move);

    if (move.type() == MOVE_PROM || board.is_capture(move))
        return score_noisy(move);

    return ordering.quiet_score(context, board, move, false);
}

template <Picker::ScorePolicy Policy>
Picker::Candidate* Picker::score_moves(const movegen::MoveList& list, Candidate* out) {
    Candidate*       cur   = out;
    Candidate* const limit = candidates.data() + candidates.size();

    for (Move move : list) {
        assert(cur < limit);
        cur->move  = move;
        cur->score = score_move<Policy>(move);
        ++cur;
    }

    return cur;
}

template <Picker::PickPolicy Policy>
bool Picker::is_pickable(const Candidate& candidate) const {
    const Move move  = candidate.move;
    const int  score = candidate.score;

    if (is_tt_move(move))
        return false;

    if constexpr (Policy == PickPolicy::Evasion) {
        return true;
    } else if constexpr (Policy == PickPolicy::GoodNoisy) {
        return move.type() == MOVE_PROM || score >= GoodCaptureScoreBase;
    } else if constexpr (Policy == PickPolicy::Quiet) {
        return !is_quiet_hint(move);
    } else {
        return move.type() != MOVE_PROM && score < GoodCaptureScoreBase;
    }
}

template <Picker::PickPolicy Policy>
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
    case Stage::PickQuietHint:
    case Stage::LoadQuiet:
    case Stage::PickQuiet:     stage = Stage::PickBadNoisy; break;

    default: break;
    }
}

Move Picker::next() {
    while (stage != Stage::Done) {
        switch (stage) {
        case Stage::TtMove:
            stage = in_check ? Stage::LoadEvasions : Stage::LoadNoisy;
            if (!tt_move.is_null())
                return tt_move;
            break;

        case Stage::LoadEvasions: {
            primary_range.next                    = candidates.data();
            const movegen::MoveList evasion_moves = movegen::generate_evasions(board);
            primary_range.end =
                score_moves<ScorePolicy::Evasion>(evasion_moves, primary_range.next);
            quiet_range.next = primary_range.end;
            quiet_range.end  = primary_range.end;
            stage            = Stage::PickEvasion;
            [[fallthrough]];
        }

        case Stage::PickEvasion: {
            Move move = pick<PickPolicy::Evasion>(primary_range);
            if (!move.is_null())
                return move;
            stage = Stage::Done;
            break;
        }

        case Stage::LoadNoisy: {
            primary_range.next                  = candidates.data();
            const movegen::MoveList noisy_moves = movegen::generate_noisy(board);
            primary_range.end = score_moves<ScorePolicy::Noisy>(noisy_moves, primary_range.next);
            quiet_range.next  = primary_range.end;
            quiet_range.end   = primary_range.end;
            stage             = Stage::PickGoodNoisy;
            [[fallthrough]];
        }

        case Stage::PickGoodNoisy: {
            Move move = pick<PickPolicy::GoodNoisy>(primary_range);
            if (!move.is_null())
                return move;

            if (mode == Mode::QSearch)
                stage = Stage::Done;
            else
                stage = skip_quiets ? Stage::PickBadNoisy : Stage::PickQuietHint;
            break;
        }

        case Stage::PickQuietHint: {
            Move move = next_quiet_hint();
            if (!move.is_null())
                return move;
            stage = Stage::LoadQuiet;
            break;
        }

        case Stage::LoadQuiet: {
            if (skip_quiets) {
                stage = Stage::PickBadNoisy;
                break;
            }
            assert(primary_range.end != nullptr);
            quiet_range.next                    = primary_range.end;
            const movegen::MoveList quiet_moves = movegen::generate_quiet(board);
            quiet_range.end = score_moves<ScorePolicy::Quiet>(quiet_moves, quiet_range.next);
            stage           = Stage::PickQuiet;
            [[fallthrough]];
        }

        case Stage::PickQuiet: {
            Move move = pick<PickPolicy::Quiet>(quiet_range);
            if (!move.is_null())
                return move;
            stage = Stage::PickBadNoisy;
            break;
        }

        case Stage::PickBadNoisy: {
            Move move = pick<PickPolicy::BadNoisy>(primary_range);
            if (!move.is_null())
                return move;
            stage = Stage::Done;
            break;
        }

        case Stage::Done: break;
        }
    }

    return NULL_MOVE;
}

} // namespace search::ordering
