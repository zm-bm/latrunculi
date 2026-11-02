#include "search/ordering/picker.hpp"

#include <cassert>
#include <utility>

#include "board/board.hpp"
#include "movegen/generator.hpp"
#include "search/ordering/capture_scoring.hpp"

namespace search::ordering {

namespace {

// Evasions share one range: promotions precede captures, which precede quiets.
constexpr int EvasionCaptureScoreBase = 1 << 22;
constexpr int PromotionScore          = EvasionCaptureScoreBase << 6;
constexpr int DeferredCaptureScore    = 0;

static_assert(QuietHistory::max_score < EvasionCaptureScoreBase);

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
    if (move.is_null() || is_quiet_hint(move) || move == tt_move)
        return NULL_MOVE;

    if (move.type() == MOVE_PROM || board.is_capture(move) || !board.is_pseudo_legal(move))
        return NULL_MOVE;

    return move;
}

template <Picker::ScorePolicy Policy>
Picker::Candidate Picker::score_move(Move move) const {
    Candidate candidate{.move = move, .deferred = false, .score = 0, .see_threshold = 0};
    if constexpr (Policy == ScorePolicy::Quiet) {
        candidate.score = state.quiet_score(context, board, move, true);
    } else {
        if (move.type() == MOVE_PROM) {
            candidate.score = PromotionScore;
            return candidate;
        }

        if constexpr (Policy == ScorePolicy::Evasion) {
            if (!board.is_capture(move)) {
                candidate.score = state.quiet_score(context, board, move, false);
                return candidate;
            }
        }

        const int victim_value   = see::piece_value(board.captured_piece_type(move));
        const int attacker_value = see::piece_value(board.piece_type_on(move.from()));
        if constexpr (Policy == ScorePolicy::Noisy) {
            const int raw_capture_history = state.capture_history_score(board, move);
            const int scaled_capture_history =
                raw_capture_history / capture_scoring::HistoryScoreDivisor;
            candidate.score =
                capture_scoring::score(victim_value, attacker_value, scaled_capture_history);
            if (mode == Mode::MainSearch)
                candidate.see_threshold = capture_scoring::main_search_see_threshold(
                    board.captured_piece_type(move), scaled_capture_history);
        } else {
            // All capture evasions precede quiet evasions without an exchange test.
            candidate.score = EvasionCaptureScoreBase
                            + capture_scoring::material_score(victim_value, attacker_value);
        }
    }
    return candidate;
}

template <Picker::ScorePolicy Policy>
Picker::Candidate* Picker::score_moves(const movegen::MoveList& list, Candidate* out) {
    Candidate*       cur   = out;
    Candidate* const limit = candidates.data() + candidates.size();

    for (Move move : list) {
        assert(cur < limit);
        *cur = score_move<Policy>(move);
        ++cur;
    }

    return cur;
}

template <typename Eligible>
Picker::Candidate* Picker::find_best(CandidateRange& range, Eligible eligible) {
    Candidate* best       = nullptr;
    int        best_score = 0;
    for (Candidate* candidate = range.next; candidate != range.end; ++candidate) {
        // Strict comparison preserves the first equal score in the current buffer order.
        const int score = candidate->score;
        if ((best == nullptr || score > best_score) && eligible(*candidate)) {
            best       = candidate;
            best_score = score;
        }
    }
    return best;
}

Move Picker::take(CandidateRange& range, Candidate* best) {
    // Keep excluded moves in the buffer: removing them changes later selection ties.
    std::swap(*range.next, *best);
    const Move move = range.next->move;
    ++range.next;
    return move;
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
            Candidate* best = find_best(primary_range, [this](const Candidate& candidate) {
                return candidate.move != tt_move;
            });
            if (best)
                return take(primary_range, best);
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
            while (Candidate* best = find_best(primary_range, [this](const Candidate& candidate) {
                       return candidate.move != tt_move && !candidate.deferred;
                   })) {
                const Move move = best->move;
                if (move.type() != MOVE_PROM && !board.see(move, best->see_threshold)) {
                    // Defer in place without swapping. Main search revisits these after
                    // quiets, with equal scores; quiescence omits them.
                    best->deferred = true;
                    best->score    = DeferredCaptureScore;
                    continue;
                }
                return take(primary_range, best);
            }

            if (mode == Mode::QSearch)
                stage = Stage::Done;
            else
                stage = Stage::PickQuietHint;
            break;
        }

        case Stage::PickQuietHint: {
            while (quiet_hint_next < quiet_hint_count) {
                Move move = quiet_hints[quiet_hint_next++];
                if (!quiet_checks_only || board.gives_check(move))
                    return move;
            }
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
            Candidate* best = find_best(quiet_range, [this](const Candidate& candidate) {
                return candidate.move != tt_move && !is_quiet_hint(candidate.move)
                    && (!quiet_checks_only || board.gives_check(candidate.move));
            });
            if (best)
                return take(quiet_range, best);
            stage = Stage::PickDeferredNoisy;
            break;
        }

        case Stage::PickDeferredNoisy: {
            Candidate* best = find_best(primary_range, [this](const Candidate& candidate) {
                return candidate.move != tt_move && candidate.deferred;
            });
            if (best)
                return take(primary_range, best);
            stage = Stage::Done;
            break;
        }

        case Stage::Done: break;
        }
    }

    return NULL_MOVE;
}

} // namespace search::ordering
