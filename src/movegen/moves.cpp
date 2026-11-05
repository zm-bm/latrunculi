#include "movegen/moves.hpp"

#include <cassert>
#include <span>
#include <utility>

#include "board/board.hpp"
#include "board/see_values.hpp"
#include "movegen/generator.hpp"
#include "search/history.hpp"

namespace movegen {

using search::History;
using search::OrderingContext;

// Candidate storage and selection.

template <Batch Which, typename Populate>
inline void CandidateBuffer::load(Populate&& populate) {
    Candidate* first = storage.data();
    if constexpr (Which == Batch::Quiet) {
        // Append after the full primary batch, including taken and excluded entries.
        assert(primary.end != nullptr);
        first = primary.end;
    }

    Candidate* end = first;
    populate([&](Candidate candidate) {
        assert(end < storage.data() + storage.size());
        *end = candidate;
        ++end;
    });
    // The producer is synchronous; publish only the completed batch.
    range_for<Which>() = {first, end};
}

template <Batch Which, CandidateState State, typename Eligible>
inline Move CandidateBuffer::take_best(Eligible eligible) {
    Candidate* selected = best<Which, State>(eligible);
    return selected ? take<Which>(selected) : NULL_MOVE;
}

template <Batch Which, CandidateState State>
inline Move CandidateBuffer::take_best() {
    return take_best<Which, State>([](const Candidate&) { return true; });
}

// Test acceptance only after ranking. Rejected candidates stay in place for later.
template <typename Accept>
inline Move CandidateBuffer::take_best_or_defer(Accept accept) {
    while (Candidate* selected = best<Batch::Primary>()) {
        if (accept(*selected))
            return take<Batch::Primary>(selected);
        defer(*selected);
    }
    return NULL_MOVE;
}

template <Batch Which>
inline CandidateBuffer::Range& CandidateBuffer::range_for() {
    if constexpr (Which == Batch::Primary)
        return primary;
    else
        return quiet;
}

template <Batch Which, CandidateState State, typename Eligible>
inline Candidate* CandidateBuffer::best(Eligible eligible) {
    auto&      range      = range_for<Which>();
    Candidate* best       = nullptr;
    int        best_score = 0;
    for (Candidate* candidate = range.next; candidate != range.end; ++candidate) {
        // Strict comparison preserves the first tie in the current buffer order.
        const int score = candidate->score;
        if ((best == nullptr || score > best_score) && candidate->state == State
            && eligible(*candidate)) {
            best       = candidate;
            best_score = score;
        }
    }
    return best;
}

template <Batch Which, CandidateState State>
inline Candidate* CandidateBuffer::best() {
    return best<Which, State>([](const Candidate&) { return true; });
}

template <Batch Which>
inline Move CandidateBuffer::take(Candidate* best) {
    auto& range = range_for<Which>();
    // Retain excluded entries: removing them changes later selection ties.
    std::swap(*range.next, *best);
    const Move move = range.next->move;
    ++range.next;
    return move;
}

inline void CandidateBuffer::defer(Candidate& candidate) {
    // Defer in place, without swapping or advancing. Deferred scores tie at zero.
    candidate.state = CandidateState::Deferred;
    candidate.score = 0;
}

// Shared scoring and batch loading.

int main_search_see_threshold(PieceType victim, int scaled_history) {
    return -(see::piece_value(victim) + scaled_history) / 2;
}

namespace {

constexpr int EvasionCaptureScoreBase = 1 << 22;
constexpr int PromotionScore          = 1 << 28;
constexpr int CaptureVictimWeight     = 7;
constexpr int CaptureMaterialOffset   = see::piece_value(QUEEN);
constexpr int CaptureHistoryDivisor   = 16;
static_assert(History::QuietMaxScore < EvasionCaptureScoreBase);
static_assert(see::piece_value(PAWN) == 100, "Capture history uses pawn=100 exchange units");

// Fixed-price material score shared by captures and capture evasions.
constexpr int capture_material_score(int victim_value, int attacker_value) {
    return CaptureVictimWeight * victim_value + CaptureMaterialOffset - attacker_value;
}

// Noisy capture scores only rank moves; stage membership is stored separately.
constexpr int capture_score(int victim_value, int attacker_value, int scaled_history) {
    return capture_material_score(victim_value, attacker_value)
         + CaptureVictimWeight * scaled_history;
}

// One synchronous batch operation shares generation and scoring across both flows.
// Excluded moves are still scored and stored; each entire batch loads before selection.
template <GenerationKind Kind, bool UseHistorySeeThreshold = false>
void load_candidates(CandidateBuffer&       candidates,
                     const Board&           board,
                     const History&         history,
                     const OrderingContext& context,
                     Move                   tt_move,
                     std::span<const Move>  quiet_hints = {}) {
    static_assert(Kind == GenerationKind::Evasions || Kind == GenerationKind::Noisy
                  || Kind == GenerationKind::Quiet);
    constexpr Batch batch = Kind == GenerationKind::Quiet ? Batch::Quiet : Batch::Primary;
    candidates.load<batch>([&](auto&& append) {
        // Only quiet scoring borrows the full context; evasions need just the side.
        generate<Kind>(
            board,
            [&,
             side          = context.side,
             quiet_context = Kind == GenerationKind::Quiet ? &context : nullptr](
                Move move, PieceType moving_piece) {
                Candidate candidate{
                    .move = move, .state = CandidateState::Ready, .score = 0, .see_threshold = 0};
                if constexpr (Kind == GenerationKind::Quiet) {
                    candidate.score = history.quiet_score(*quiet_context, moving_piece, move);
                } else if (move.type() == MOVE_PROM) {
                    candidate.score = PromotionScore;
                } else if constexpr (Kind == GenerationKind::Evasions) {
                    if (board.is_capture(move)) {
                        const int victim   = see::piece_value(board.captured_piece_type(move));
                        const int attacker = see::piece_value(moving_piece);
                        candidate.score =
                            EvasionCaptureScoreBase + capture_material_score(victim, attacker);
                    } else {
                        candidate.score = history.base_quiet_score(side, move);
                    }
                } else {
                    const PieceType victim         = board.captured_piece_type(move);
                    const int       victim_value   = see::piece_value(victim);
                    const int       attacker_value = see::piece_value(moving_piece);
                    const int       raw_history    = history.capture_history_score(
                        board.side_to_move(), moving_piece, move.to(), victim);
                    const int scaled_history = raw_history / CaptureHistoryDivisor;
                    candidate.score = capture_score(victim_value, attacker_value, scaled_history);
                    if constexpr (UseHistorySeeThreshold)
                        candidate.see_threshold = main_search_see_threshold(victim, scaled_history);
                }
                bool excluded = move == tt_move;
                if constexpr (Kind == GenerationKind::Quiet) {
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

Move validate_tt_hint(const Board& board, Move move, bool in_check) {
    if (move.is_null() || !board.is_pseudo_legal(move))
        return NULL_MOVE;
    if (in_check && !board.is_legal_pseudo_move(move))
        return NULL_MOVE;
    return move;
}

} // namespace

// Unordered traversal.

MoveSource<MovePolicy::Unordered>::MoveSource(const Board& board)
    : board(board),
      cursor(storage.data()) {
    Move* fill = cursor;
    generate_pseudo_legal(board, [&](Move move, PieceType) {
        assert(fill < storage.data() + storage.size());
        *fill++ = move;
    });
    end = fill;
}

// Main-search ordering.

MoveSource<MovePolicy::MainSearch>::MoveSource(const Board&           board,
                                               const History&         history,
                                               const OrderingContext& context)
    : board(board),
      history(history),
      context(context) {
    tt_move = validate_tt_hint(board, context.tt_move, context.in_check);
    if (!context.in_check) {
        for (Move move : {context.primary_killer, context.secondary_killer, context.counter_move}) {
            if (move.is_null() || is_quiet_hint(move) || move == tt_move)
                continue;
            if (move.type() == MOVE_PROM || board.is_capture(move) || !board.is_pseudo_legal(move))
                continue;
            assert(quiet_hint_count < QuietHintCapacity);
            quiet_hints[quiet_hint_count++] = move;
        }
    }
}

bool MoveSource<MovePolicy::MainSearch>::is_quiet_hint(Move move) const {
    for (int i = 0; i < quiet_hint_count; ++i)
        if (quiet_hints[i] == move)
            return true;
    return false;
}

void MoveSource<MovePolicy::MainSearch>::skip_nonchecking_quiets() {
    if (!context.in_check)
        quiet_checks_only = true;
}

Move MoveSource<MovePolicy::MainSearch>::next_candidate() {
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
            if (context.in_check) {
                load_candidates<GenerationKind::Evasions>(
                    candidates, board, history, context, tt_move);
                stage = Stage::PickEvasion;
                break;
            }
            load_candidates<GenerationKind::Noisy, true>(
                candidates, board, history, context, tt_move);
            stage = Stage::PickEarlyNoisy;
            [[fallthrough]];
        case Stage::PickEarlyNoisy: {
            const Move move = candidates.take_best_or_defer([this](const Candidate& candidate) {
                const Move move = candidate.move;
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
            load_candidates<GenerationKind::Quiet>(
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
                    return !quiet_checks_only || board.gives_check(candidate.move);
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

// Quiescence ordering.

MoveSource<MovePolicy::Quiescence>::MoveSource(const Board& board, const History& history, Move tt)
    : board(board),
      history(history),
      side(board.side_to_move()),
      in_check(board.is_check()) {
    tt_move = validate_tt_hint(board, tt, in_check);
    if (!in_check && !tt_move.is_null() && tt_move.type() != MOVE_PROM
        && !board.is_capture(tt_move))
        tt_move = NULL_MOVE;
}

Move MoveSource<MovePolicy::Quiescence>::next_candidate() {
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
                load_candidates<GenerationKind::Evasions>(
                    candidates, board, history, OrderingContext{.side = side}, tt_move);
                stage = Stage::PickEvasion;
                break;
            }
            load_candidates<GenerationKind::Noisy>(
                candidates, board, history, OrderingContext{.side = side}, tt_move);
            stage = Stage::PickNoisy;
            [[fallthrough]];
        case Stage::PickNoisy: {
            const Move move = candidates.take_best_or_defer([this](const Candidate& candidate) {
                return candidate.move.type() == MOVE_PROM || board.see(candidate.move, 0);
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

} // namespace movegen
