#include "search/ordering/picker.hpp"

#include <cassert>
#include <span>

#include "board/board.hpp"
#include "movegen/generator_detail.hpp"
#include "search/ordering/capture_scoring.hpp"

namespace search::ordering {

namespace {

using detail::Batch;
using detail::Candidate;
using detail::CandidateBuffer;
using detail::CandidateState;
using movegen::MoveGenType;

constexpr int EvasionCaptureScoreBase = 1 << 22;
constexpr int PromotionScore          = 1 << 28;
static_assert(History::QuietMaxScore < EvasionCaptureScoreBase);

Move validate_tt_hint(const Board& board, Move move, bool in_check) {
    if (move.is_null() || !board.is_pseudo_legal(move))
        return NULL_MOVE;
    if (in_check && !board.is_legal_pseudo_move(move))
        return NULL_MOVE;
    return move;
}

// One synchronous batch operation shares generation and scoring across both flows.
// Excluded moves are still scored and stored; each entire batch loads before selection.
template <MoveGenType Kind, bool HistorySee = false>
void load_candidates(CandidateBuffer&      candidates,
                     const Board&          board,
                     const History&        history,
                     const NodeContext&    context,
                     Move                  tt_move,
                     std::span<const Move> quiet_hints = {}) {
    static_assert(Kind == MoveGenType::Evasions || Kind == MoveGenType::Noisy
                  || Kind == MoveGenType::Quiet);
    assert((Kind == MoveGenType::Evasions) == board.is_check());
    constexpr Batch batch = Kind == MoveGenType::Quiet ? Batch::Quiet : Batch::Primary;
    candidates.load<batch>([&](auto&& append) {
        movegen::detail::generate<Kind>(board, [&](Move move, PieceType moving_piece) {
            Candidate candidate{.move_bits     = move.bits,
                                .state         = CandidateState::Ready,
                                .score         = 0,
                                .see_threshold = 0};
            if constexpr (Kind == MoveGenType::Quiet) {
                candidate.score = history.quiet_score(context, moving_piece, move);
            } else if (move.type() == MOVE_PROM) {
                candidate.score = PromotionScore;
            } else if constexpr (Kind == MoveGenType::Evasions) {
                if (board.is_capture(move)) {
                    const int victim   = see::piece_value(board.captured_piece_type(move));
                    const int attacker = see::piece_value(moving_piece);
                    candidate.score =
                        EvasionCaptureScoreBase + capture_scoring::material_score(victim, attacker);
                } else {
                    candidate.score = history.base_quiet_score(context.side, move);
                }
            } else {
                const PieceType victim         = board.captured_piece_type(move);
                const int       victim_value   = see::piece_value(victim);
                const int       attacker_value = see::piece_value(moving_piece);
                const int       raw_history    = history.capture_history_score(
                    board.side_to_move(), moving_piece, move.to(), victim);
                const int scaled_history = raw_history / capture_scoring::HistoryScoreDivisor;
                candidate.score =
                    capture_scoring::score(victim_value, attacker_value, scaled_history);
                if constexpr (HistorySee)
                    candidate.see_threshold =
                        capture_scoring::main_search_see_threshold(victim, scaled_history);
            }
            bool excluded = move == tt_move;
            if constexpr (Kind == MoveGenType::Quiet) {
                if (!excluded) {
                    for (Move hint : quiet_hints) {
                        if (move == hint) {
                            excluded = true;
                            break;
                        }
                    }
                }
            }
            if (excluded)
                candidate.state = CandidateState::Excluded;
            append(candidate);
        });
    });
}

} // namespace

MainPicker::MainPicker(
    const Board& board, const History& history, const NodeContext& context, int ply, Move tt_move)
    : MainPicker(board,
                 history,
                 context,
                 tt_move,
                 {.primary_killer   = history.primary_killer(ply),
                  .secondary_killer = history.secondary_killer(ply),
                  .counter          = history.counter_hint(context)}) {}

MainPicker::MainPicker(const Board&        board,
                       const History&      history,
                       const NodeContext&  context,
                       Move                tt,
                       QuietHintCandidates quiet_hint_candidates)
    : board(board),
      history(history),
      context(context),
      counter_move(quiet_hint_candidates.counter),
      in_check(board.is_check()) {
    tt_move = validate_tt_hint(board, tt, in_check);
    if (!in_check) {
        for (Move move : {quiet_hint_candidates.primary_killer,
                          quiet_hint_candidates.secondary_killer,
                          quiet_hint_candidates.counter}) {
            if (move.is_null() || is_quiet_hint(move) || move == tt_move)
                continue;
            if (move.type() == MOVE_PROM || board.is_capture(move) || !board.is_pseudo_legal(move))
                continue;
            assert(quiet_hint_count < QuietHintCapacity);
            quiet_hints[quiet_hint_count++] = move;
        }
    }
}

bool MainPicker::is_quiet_hint(Move move) const {
    if (move.is_null())
        return false;
    for (int i = 0; i < quiet_hint_count; ++i)
        if (quiet_hints[i] == move)
            return true;
    return false;
}

void MainPicker::skip_nonchecking_quiets() {
    if (!in_check)
        quiet_checks_only = true;
}

Move MainPicker::next_candidate() {
    while (stage != Stage::Done) {
        switch (stage) {
        case Stage::TtMove:
            stage = Stage::LoadPrimary;
            if (!tt_move.is_null())
                return tt_move;
            break;
        case Stage::PickEvasion:
            if (Move move = candidates.take_best<Batch::Primary>(); !move.is_null())
                return move;
            stage = Stage::Done;
            break;
        case Stage::LoadPrimary:
            if (in_check) {
                load_candidates<MoveGenType::Evasions>(
                    candidates, board, history, context, tt_move);
                stage = Stage::PickEvasion;
                break;
            }
            load_candidates<MoveGenType::Noisy, true>(candidates, board, history, context, tt_move);
            stage = Stage::PickEarlyNoisy;
            [[fallthrough]];
        case Stage::PickEarlyNoisy: {
            const Move move = candidates.take_best_or_defer([this](const Candidate& candidate) {
                const Move move = candidate.move();
                return move.type() == MOVE_PROM || board.see(move, candidate.see_threshold);
            });
            if (!move.is_null())
                return move;
            stage = Stage::PickQuietHint;
            break;
        }
        case Stage::PickQuietHint:
            while (quiet_hint_next < quiet_hint_count) {
                Move move = quiet_hints[quiet_hint_next++];
                if (!quiet_checks_only || board.gives_check(move))
                    return move;
            }
            stage = Stage::LoadQuiet;
            break;
        case Stage::LoadQuiet:
            load_candidates<MoveGenType::Quiet>(
                candidates,
                board,
                history,
                context,
                tt_move,
                {quiet_hints.data(), static_cast<std::size_t>(quiet_hint_count)});
            stage = Stage::PickQuiet;
            [[fallthrough]];
        case Stage::PickQuiet: {
            const Move move =
                candidates.take_best<Batch::Quiet>([this](const Candidate& candidate) {
                    return !quiet_checks_only || board.gives_check(candidate.move());
                });
            if (!move.is_null())
                return move;
            stage = Stage::PickDeferredNoisy;
            break;
        }
        case Stage::PickDeferredNoisy:
            if (Move move = candidates.take_best<Batch::Primary, CandidateState::Deferred>();
                !move.is_null())
                return move;
            stage = Stage::Done;
            break;
        case Stage::Done: break;
        }
    }
    return NULL_MOVE;
}

QuiescencePicker::QuiescencePicker(const Board& board, const History& history, Move tt)
    : board(board),
      history(history),
      context{.side = board.side_to_move()},
      in_check(board.is_check()) {
    tt_move = validate_tt_hint(board, tt, in_check);
    if (!in_check && !tt_move.is_null() && tt_move.type() != MOVE_PROM
        && !board.is_capture(tt_move))
        tt_move = NULL_MOVE;
}

Move QuiescencePicker::next_candidate() {
    while (stage != Stage::Done) {
        switch (stage) {
        case Stage::TtMove:
            stage = Stage::LoadPrimary;
            if (!tt_move.is_null())
                return tt_move;
            break;
        case Stage::PickEvasion:
            if (Move move = candidates.take_best<Batch::Primary>(); !move.is_null())
                return move;
            stage = Stage::Done;
            break;
        case Stage::LoadPrimary:
            if (in_check) {
                load_candidates<MoveGenType::Evasions>(
                    candidates, board, history, context, tt_move);
                stage = Stage::PickEvasion;
                break;
            }
            load_candidates<MoveGenType::Noisy>(candidates, board, history, context, tt_move);
            stage = Stage::PickNoisy;
            [[fallthrough]];
        case Stage::PickNoisy: {
            const Move move = candidates.take_best_or_defer([this](const Candidate& candidate) {
                return candidate.move().type() == MOVE_PROM || board.see(candidate.move(), 0);
            });
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
