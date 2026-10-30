#include "search/ordering/picker.hpp"

#include <cassert>
#include <utility>

#include "board/board.hpp"
#include "eval/parameters.hpp"
#include "movegen/generator.hpp"
#include "search/ordering/capture_scoring.hpp"

namespace search::ordering {

namespace {

// Coarse score bands, not tuned chess values. Unclassified captures start in
// the early band; the selected capture is checked by SEE before it is returned.
// In-band terms must not cross stage boundaries.
constexpr int EarlyCaptureScoreBase = 1 << 22;
constexpr int PromotionScore        = EarlyCaptureScoreBase << 6;
constexpr int DeferredCaptureScore  = 0;

// Evasions retain their numerical SEE scoring and evaluation-price weighting.
constexpr int EvasionVictimWeight = 7;

static_assert(QuietHistory::max_score + ContinuationHistory::max_score < EarlyCaptureScoreBase);
static_assert(capture_scoring::HistoryScoreBias
              == capture_scoring::VictimWeight
                     * (CaptureHistory::max_score / capture_scoring::HistoryScoreDivisor));

} // namespace

Picker Picker::for_main_search(
    const Board& board, const State& state, const State::Context& context, int ply, Move tt_move) {
    const Picker::QuietHintCandidates quiet_hint_candidates{
        state.killers.primary(ply),
        state.killers.secondary(ply),
        state.counter_hint(context),
    };

    return Picker(Picker::Mode::MainSearch, board, state, context, tt_move, quiet_hint_candidates);
}

Picker Picker::for_quiescence(const Board& board, const State& state, Move tt_move) {
    const State::Context context{.side = board.side_to_move()};
    return Picker(Picker::Mode::QSearch, board, state, context, tt_move);
}

Picker::Picker(Mode                  mode,
               const Board&          board,
               const State&          state,
               const State::Context& context,
               Move                  tt,
               QuietHintCandidates   quiet_hint_candidates)
    : board(board),
      state(state),
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
    while (quiet_hint_next < quiet_hint_count) {
        Move move = quiet_hints[quiet_hint_next];
        ++quiet_hint_next;
        if (!quiet_checks_only || board.gives_check(move))
            return move;
    }

    return NULL_MOVE;
}

int Picker::score_evasion_noisy(Move move) const {
    if (move.type() == MOVE_PROM)
        return PromotionScore;

    const int see_score = board.see(move);
    if (see_score < 0)
        return DeferredCaptureScore;

    const int victim_value = eval::piece(board.captured_piece_type(move)).mg;
    return EarlyCaptureScoreBase + EvasionVictimWeight * victim_value + see_score;
}

template <Picker::ScorePolicy Policy>
int Picker::score_move(Move move) const {
    if constexpr (Policy == ScorePolicy::Quiet)
        return state.quiet_score(context, board, move, true);

    if constexpr (Policy == ScorePolicy::Noisy) {
        if (move.type() == MOVE_PROM)
            return PromotionScore;

        const int victim_value        = see::piece_value(board.captured_piece_type(move));
        const int attacker_value      = see::piece_value(board.piece_type_on(move.from()));
        const int raw_capture_history = state.capture_score(board, move);
        const int scaled_capture_history =
            raw_capture_history / capture_scoring::HistoryScoreDivisor;
        return EarlyCaptureScoreBase
             + capture_scoring::score(victim_value, attacker_value, scaled_capture_history);
    }

    if (move.type() == MOVE_PROM || board.is_capture(move))
        return score_evasion_noisy(move);

    return state.quiet_score(context, board, move, false);
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
    } else if constexpr (Policy == PickPolicy::EarlyNoisy) {
        return move.type() == MOVE_PROM || score >= EarlyCaptureScoreBase;
    } else if constexpr (Policy == PickPolicy::Quiet) {
        return !is_quiet_hint(move) && (!quiet_checks_only || board.gives_check(move));
    } else {
        return move.type() != MOVE_PROM && score < EarlyCaptureScoreBase;
    }
}

template <Picker::PickPolicy Policy>
Move Picker::pick(CandidateRange& range) {
    for (;;) {
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

        if constexpr (Policy == PickPolicy::EarlyNoisy) {
            int see_threshold = 0;
            if (best->move.type() != MOVE_PROM && mode == Mode::MainSearch) {
                const int victim   = see::piece_value(board.captured_piece_type(best->move));
                const int attacker = see::piece_value(board.piece_type_on(best->move.from()));
                // Recover the history snapshot from the score, without rereading a table
                // that may have changed while an earlier move was being searched.
                const int scaled_capture_history = capture_scoring::scaled_history_from_score(
                    best->score - EarlyCaptureScoreBase, victim, attacker);
                see_threshold = capture_scoring::main_search_see_threshold(
                    board.captured_piece_type(best->move), scaled_capture_history);
            }
            if (best->move.type() != MOVE_PROM && !board.see_ge(best->move, see_threshold)) {
                // Captures below the threshold remain for the final main-search
                // stage. Quiescence ends after the early captures.
                best->score = DeferredCaptureScore;
                continue;
            }
        }

        std::swap(*range.next, *best);
        Move move = range.next->move;
        ++range.next;
        return move;
    }
}

void Picker::skip_nonchecking_quiets() {
    if (mode != Mode::MainSearch || in_check)
        return;

    quiet_checks_only = true;
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
            stage             = Stage::PickEarlyNoisy;
            [[fallthrough]];
        }

        case Stage::PickEarlyNoisy: {
            Move move = pick<PickPolicy::EarlyNoisy>(primary_range);
            if (!move.is_null())
                return move;

            if (mode == Mode::QSearch)
                stage = Stage::Done;
            else
                stage = Stage::PickQuietHint;
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
            stage = Stage::PickDeferredNoisy;
            break;
        }

        case Stage::PickDeferredNoisy: {
            Move move = pick<PickPolicy::DeferredNoisy>(primary_range);
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
