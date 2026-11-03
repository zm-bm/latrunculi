#include "search/ordering/picker.hpp"

#include <cassert>

#include "board/board.hpp"
#include "movegen/generator.hpp"
#include "search/ordering/capture_scoring.hpp"

namespace search::ordering {

namespace {

// Evasions share one range: promotions precede captures, which precede quiets.
constexpr int EvasionCaptureScoreBase = 1 << 22;
constexpr int PromotionScore          = 1 << 28;

static_assert(History::QuietMaxScore < EvasionCaptureScoreBase);

} // namespace

template <PickerMode Mode>
Picker<Mode>::Picker(
    const Board& board, const History& history, const NodeContext& context, int ply, Move tt_move)
    requires(Mode == PickerMode::MainSearch)
    : Picker(board,
             history,
             context,
             tt_move,
             {.primary_killer   = history.primary_killer(ply),
              .secondary_killer = history.secondary_killer(ply),
              .counter          = history.counter_hint(context)}) {}

template <PickerMode Mode>
Picker<Mode>::Picker(const Board& board, const History& history, Move tt_move)
    requires(Mode == PickerMode::Quiescence)
    : Picker(board, history, NodeContext{.side = board.side_to_move()}, tt_move) {}

template <PickerMode Mode>
Picker<Mode>::Picker(const Board&        board,
                     const History&      history,
                     const NodeContext&  context,
                     Move                tt,
                     QuietHintCandidates quiet_hint_candidates)
    : board(board),
      history(history),
      context(context),
      counter_move(quiet_hint_candidates.counter),
      in_check(board.is_check()) {
    tt_move = validate_tt_hint(tt);

    if constexpr (Mode == PickerMode::MainSearch) {
        if (!in_check) {
            for (Move move : {quiet_hint_candidates.primary_killer,
                              quiet_hint_candidates.secondary_killer,
                              quiet_hint_candidates.counter}) {
                if (move.is_null() || is_quiet_hint(move) || move == tt_move)
                    continue;

                if (move.type() == MOVE_PROM || board.is_capture(move)
                    || !board.is_pseudo_legal(move))
                    continue;

                assert(quiet_hint_count < QuietHintCapacity);
                quiet_hints[quiet_hint_count++] = move;
            }
        }
    }
}

template <PickerMode Mode>
bool Picker<Mode>::is_quiet_hint(Move move) const {
    if (move.is_null())
        return false;

    for (int i = 0; i < quiet_hint_count; ++i) {
        if (quiet_hints[i] == move)
            return true;
    }

    return false;
}

template <PickerMode Mode>
Move Picker<Mode>::validate_tt_hint(Move move) const {
    if (move.is_null() || !board.is_pseudo_legal(move))
        return NULL_MOVE;

    if (in_check && !board.is_legal_pseudo_move(move))
        return NULL_MOVE;

    if constexpr (Mode == PickerMode::Quiescence) {
        if (!in_check && move.type() != MOVE_PROM && !board.is_capture(move))
            return NULL_MOVE;
    }

    return move;
}

template <PickerMode Mode>
template <movegen::MoveGenType Kind>
void Picker<Mode>::load() {
    static_assert(Kind == movegen::MoveGenType::Evasions || Kind == movegen::MoveGenType::Noisy
                  || Kind == movegen::MoveGenType::Quiet);
    static_assert(Mode == PickerMode::MainSearch || Kind != movegen::MoveGenType::Quiet);
    assert((Kind == movegen::MoveGenType::Evasions) == board.is_check());
    constexpr Batch batch = Kind == movegen::MoveGenType::Quiet ? Batch::Quiet : Batch::Primary;
    candidates.load<batch>([this](auto&& append) {
        movegen::generate<Kind>(board, [&](Move move) { append(score_move<Kind>(move)); });
    });
}

template <PickerMode Mode>
template <movegen::MoveGenType Kind>
typename Picker<Mode>::Candidate Picker<Mode>::score_move(Move move) const {
    Candidate candidate{
        .move_bits = move.bits, .state = CandidateState::Ready, .score = 0, .see_threshold = 0};
    if constexpr (Kind == movegen::MoveGenType::Quiet) {
        candidate.score = history.quiet_score(context, board, move);
    } else if (move.type() == MOVE_PROM) {
        candidate.score = PromotionScore;
    } else if constexpr (Kind == movegen::MoveGenType::Evasions) {
        if (board.is_capture(move)) {
            // All capture evasions precede quiet evasions without an exchange test.
            const int victim_value   = see::piece_value(board.captured_piece_type(move));
            const int attacker_value = see::piece_value(board.piece_type_on(move.from()));
            candidate.score          = EvasionCaptureScoreBase
                            + capture_scoring::material_score(victim_value, attacker_value);
        } else {
            candidate.score = history.base_quiet_score(context.side, move);
        }
    } else {
        const int victim_value        = see::piece_value(board.captured_piece_type(move));
        const int attacker_value      = see::piece_value(board.piece_type_on(move.from()));
        const int raw_capture_history = history.capture_history_score(board, move);
        const int scaled_capture_history =
            raw_capture_history / capture_scoring::HistoryScoreDivisor;
        candidate.score =
            capture_scoring::score(victim_value, attacker_value, scaled_capture_history);
        if constexpr (Mode == PickerMode::MainSearch)
            candidate.see_threshold = capture_scoring::main_search_see_threshold(
                board.captured_piece_type(move), scaled_capture_history);
    }

    // Excluded entries still receive scores and occupy their original slots.
    if (move == tt_move || (Kind == movegen::MoveGenType::Quiet && is_quiet_hint(move)))
        candidate.state = CandidateState::Excluded;
    return candidate;
}

template <PickerMode Mode>
void Picker<Mode>::skip_nonchecking_quiets() requires(Mode == PickerMode::MainSearch)
{
    if (in_check)
        return;

    quiet_checks_only = true;
}

template <PickerMode Mode>
Move Picker<Mode>::next_candidate() {
    while (stage != Stage::Done) {
        switch (stage) {
        case Stage::TtMove:
            stage = in_check ? Stage::LoadEvasions : Stage::LoadNoisy;
            if (!tt_move.is_null())
                return tt_move;
            break;

        case Stage::LoadEvasions: {
            load<movegen::MoveGenType::Evasions>();
            stage = Stage::PickEvasion;
            [[fallthrough]];
        }

        case Stage::PickEvasion: {
            Candidate* best = candidates.best<Batch::Primary>();
            if (best)
                return candidates.take<Batch::Primary>(best);
            stage = Stage::Done;
            break;
        }

        case Stage::LoadNoisy: {
            load<movegen::MoveGenType::Noisy>();
            stage = Stage::PickEarlyNoisy;
            [[fallthrough]];
        }

        case Stage::PickEarlyNoisy: {
            while (Candidate* best = candidates.best<Batch::Primary>()) {
                const Move move = best->move();
                if (move.type() != MOVE_PROM && !board.see(move, best->see_threshold)) {
                    // Main search revisits these after quiets; quiescence omits them.
                    candidates.defer(*best);
                    continue;
                }
                return candidates.take<Batch::Primary>(best);
            }

            if constexpr (Mode == PickerMode::Quiescence)
                stage = Stage::Done;
            else
                stage = Stage::PickQuietHint;
            break;
        }

        case Stage::PickQuietHint: {
            if constexpr (Mode == PickerMode::MainSearch) {
                while (quiet_hint_next < quiet_hint_count) {
                    Move move = quiet_hints[quiet_hint_next++];
                    if (!quiet_checks_only || board.gives_check(move))
                        return move;
                }
                stage = Stage::LoadQuiet;
            } else {
                stage = Stage::Done;
            }
            break;
        }

        case Stage::LoadQuiet: {
            if constexpr (Mode == PickerMode::MainSearch) {
                load<movegen::MoveGenType::Quiet>();
                stage = Stage::PickQuiet;
            } else {
                stage = Stage::Done;
                break;
            }
            [[fallthrough]];
        }

        case Stage::PickQuiet: {
            if constexpr (Mode == PickerMode::MainSearch) {
                Candidate* best = candidates.best<Batch::Quiet>([this](const Candidate& candidate) {
                    const Move move = candidate.move();
                    return !quiet_checks_only || board.gives_check(move);
                });
                if (best)
                    return candidates.take<Batch::Quiet>(best);
                stage = Stage::PickDeferredNoisy;
            } else {
                stage = Stage::Done;
            }
            break;
        }

        case Stage::PickDeferredNoisy: {
            if constexpr (Mode == PickerMode::MainSearch) {
                Candidate* best = candidates.best<Batch::Primary, CandidateState::Deferred>();
                if (best)
                    return candidates.take<Batch::Primary>(best);
            }
            stage = Stage::Done;
            break;
        }

        case Stage::Done: break;
        }
    }

    return NULL_MOVE;
}

template class Picker<PickerMode::MainSearch>;
template class Picker<PickerMode::Quiescence>;

} // namespace search::ordering
