#include "movegen/move_sources.hpp"

#include <algorithm>
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

// Candidate scoring.

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

int score_evasion(
    const Board& board, const History& history, Color side, Move move, PieceType moving_piece) {
    if (move.type() == MOVE_PROM)
        return PromotionScore;
    if (board.is_capture(move)) {
        const int victim_value   = see::piece_value(board.captured_piece_type(move));
        const int attacker_value = see::piece_value(moving_piece);
        return EvasionCaptureScoreBase + capture_material_score(victim_value, attacker_value);
    }
    return history.base_quiet_score(side, move);
}

// Ranking and SEE staging share one history snapshot but serve different purposes.
struct NoisyScore {
    int score;
    int see_threshold;
};

template <MovePolicy Policy>
NoisyScore
score_noisy(const Board& board, const History& history, Move move, PieceType moving_piece) {
    static_assert(Policy == MovePolicy::MainSearch || Policy == MovePolicy::Quiescence);
    if (move.type() == MOVE_PROM)
        return {.score = PromotionScore, .see_threshold = 0};

    const PieceType victim         = board.captured_piece_type(move);
    const int       victim_value   = see::piece_value(victim);
    const int       attacker_value = see::piece_value(moving_piece);
    const int       raw_history =
        history.capture_score(board.side_to_move(), moving_piece, move.to(), victim);
    const int  scaled_history = raw_history / CaptureHistoryDivisor;
    NoisyScore result{.score = capture_material_score(victim_value, attacker_value)
                             + CaptureVictimWeight * scaled_history,
                      .see_threshold = 0};
    // Divide after combining material and history to preserve signed truncation.
    if constexpr (Policy == MovePolicy::MainSearch)
        result.see_threshold = -(victim_value + scaled_history) / 2;
    return result;
}

} // namespace

// Batch loading.

// Each loader scores even excluded entries and publishes only after generation finishes.
void OrderedCandidates::load_evasions(const Board&   board,
                                      const History& history,
                                      Color          side,
                                      Move           tt_hint) {
    Candidate* const first = storage.data();
    Candidate*       fill  = first;
    generate_pseudo_legal<GenerationKind::Evasions>(
        board, [&, side](Move move, PieceType moving_piece) {
            Candidate candidate{.move  = move,
                                .state = CandidateState::Ready,
                                .score = score_evasion(board, history, side, move, moving_piece),
                                .see_threshold = 0};
            if (move == tt_hint)
                candidate.state = CandidateState::Excluded;
            assert(fill < storage.data() + storage.size());
            *fill++ = candidate;
        });
    primary = {first, fill};
}

template <MovePolicy Policy>
void OrderedCandidates::load_noisy(const Board& board, const History& history, Move tt_hint) {
    Candidate* const first = storage.data();
    Candidate*       fill  = first;
    generate_pseudo_legal<GenerationKind::Noisy>(board, [&](Move move, PieceType moving_piece) {
        const NoisyScore scoring = score_noisy<Policy>(board, history, move, moving_piece);
        Candidate        candidate{.move          = move,
                                   .state         = CandidateState::Ready,
                                   .score         = scoring.score,
                                   .see_threshold = scoring.see_threshold};
        if (move == tt_hint)
            candidate.state = CandidateState::Excluded;
        assert(fill < storage.data() + storage.size());
        *fill++ = candidate;
    });
    primary = {first, fill};
}

void OrderedCandidates::load_quiets(const Board&           board,
                                    const History&         history,
                                    const OrderingContext& context,
                                    Move                   tt_hint,
                                    std::span<const Move>  quiet_hints) {
    // Append after the full primary batch, including taken and excluded entries.
    assert(primary.end != nullptr);
    Candidate* const first = primary.end;
    Candidate*       fill  = first;
    generate_pseudo_legal<GenerationKind::Quiet>(board, [&](Move move, PieceType moving_piece) {
        Candidate candidate{.move          = move,
                            .state         = CandidateState::Ready,
                            .score         = history.quiet_score(context, moving_piece, move),
                            .see_threshold = 0};
        if (move == tt_hint || std::ranges::contains(quiet_hints, move))
            candidate.state = CandidateState::Excluded;
        assert(fill < storage.data() + storage.size());
        *fill++ = candidate;
    });
    quiet = {first, fill};
}

// Candidate storage and selection.

inline Move OrderedCandidates::take_evasion() {
    return consume(primary, find_best_ready(primary, [](Move) { return true; }));
}

// Test acceptance only after ranking. Rejected candidates stay in place for later.
template <typename Accept>
inline Move OrderedCandidates::take_noisy(Accept accept) {
    while (Candidate* selected = find_best_ready(primary, [](Move) { return true; })) {
        if (accept(selected->move, selected->see_threshold))
            return consume(primary, selected);
        // Defer in place, without swapping or advancing; retain the scoring snapshot.
        selected->state = CandidateState::Deferred;
    }
    return NULL_MOVE;
}

template <typename Eligible>
inline Move OrderedCandidates::take_quiet(Eligible eligible) {
    return consume(quiet, find_best_ready(quiet, eligible));
}

inline Move OrderedCandidates::take_deferred_noisy() {
    // Deferred moves follow the current buffer order, regardless of their scores.
    for (Candidate* candidate = primary.cursor; candidate != primary.end; ++candidate)
        if (candidate->state == CandidateState::Deferred)
            return consume(primary, candidate);
    return NULL_MOVE;
}

template <typename Eligible>
inline OrderedCandidates::Candidate* OrderedCandidates::find_best_ready(const Range& range,
                                                                        Eligible     eligible) {
    Candidate* best       = nullptr;
    int        best_score = 0;
    for (Candidate* candidate = range.cursor; candidate != range.end; ++candidate) {
        // Strict comparison preserves the first tie in the current buffer order.
        const int score = candidate->score;
        if ((best == nullptr || score > best_score) && candidate->state == CandidateState::Ready
            && eligible(candidate->move)) {
            best       = candidate;
            best_score = score;
        }
    }
    return best;
}

inline Move OrderedCandidates::consume(Range& range, Candidate* selected) {
    if (selected == nullptr)
        return NULL_MOVE;
    // Retain excluded entries: removing them changes later selection ties.
    std::swap(*range.cursor, *selected);
    const Move move = range.cursor->move;
    ++range.cursor;
    return move;
}

namespace {

// Outside check, final legality remains the stream's responsibility.
Move validate_tt_hint(const Board& board, Move tt_move, bool in_check) {
    if (tt_move.is_null() || !board.is_pseudo_legal(tt_move))
        return NULL_MOVE;
    if (in_check && !board.is_legal_pseudo_move(tt_move))
        return NULL_MOVE;
    return tt_move;
}

} // namespace

// Unscored traversal.

MoveSource<MovePolicy::Unscored>::MoveSource(const Board& board)
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
    tt_hint = validate_tt_hint(board, context.tt_move, context.in_check);
    if (!context.in_check) {
        for (Move move : {context.primary_killer, context.secondary_killer, context.counter_move}) {
            if (move.is_null()
                || std::ranges::contains(std::span(quiet_hints).first(quiet_hint_count), move)
                || move == tt_hint)
                continue;
            if (move.type() == MOVE_PROM || board.is_capture(move) || !board.is_pseudo_legal(move))
                continue;
            assert(quiet_hint_count < QuietHintCapacity);
            quiet_hints[quiet_hint_count++] = move;
        }
    }
}

Move MoveSource<MovePolicy::MainSearch>::next_candidate() {
    while (stage != Stage::Done) {
        switch (stage) {
        case Stage::TtHint:
            stage = Stage::LoadPrimary;
            if (!tt_hint.is_null())
                return tt_hint;
            break;
        case Stage::Evasions:
            if (Move move = candidates.take_evasion(); !move.is_null())
                return move;
            stage = Stage::Done;
            break;
        case Stage::LoadPrimary:
            if (context.in_check) {
                candidates.load_evasions(board, history, context.side, tt_hint);
                stage = Stage::Evasions;
                break;
            }
            candidates.load_noisy<MovePolicy::MainSearch>(board, history, tt_hint);
            stage = Stage::EarlyNoisy;
            [[fallthrough]];
        case Stage::EarlyNoisy: {
            const Move move = candidates.take_noisy([this](Move move, int see_threshold) {
                return move.type() == MOVE_PROM || board.see(move, see_threshold);
            });
            if (!move.is_null())
                return move;
            stage = Stage::QuietHints;
            break;
        }
        case Stage::QuietHints:
            while (quiet_hint_index < quiet_hint_count) {
                Move move = quiet_hints[quiet_hint_index++];
                if (!quiets_must_check || board.gives_check(move))
                    return move;
            }
            candidates.load_quiets(
                board,
                history,
                context,
                tt_hint,
                {quiet_hints.data(), static_cast<std::size_t>(quiet_hint_count)});
            stage = Stage::Quiets;
            [[fallthrough]];
        case Stage::Quiets: {
            const Move move = candidates.take_quiet(
                [this](Move move) { return !quiets_must_check || board.gives_check(move); });
            if (!move.is_null())
                return move;
            stage = Stage::DeferredNoisy;
            break;
        }
        case Stage::DeferredNoisy:
            if (Move move = candidates.take_deferred_noisy(); !move.is_null())
                return move;
            stage = Stage::Done;
            break;
        case Stage::Done: break;
        }
    }
    return NULL_MOVE;
}

void MoveSource<MovePolicy::MainSearch>::skip_nonchecking_quiets() {
    if (!context.in_check)
        quiets_must_check = true;
}

// Quiescence ordering.

MoveSource<MovePolicy::Quiescence>::MoveSource(const Board&   board,
                                               const History& history,
                                               Move           tt_move)
    : board(board),
      history(history),
      side(board.side_to_move()),
      in_check(board.is_check()) {
    tt_hint = validate_tt_hint(board, tt_move, in_check);
    if (!in_check && !tt_hint.is_null() && tt_hint.type() != MOVE_PROM
        && !board.is_capture(tt_hint))
        tt_hint = NULL_MOVE;
}

Move MoveSource<MovePolicy::Quiescence>::next_candidate() {
    while (stage != Stage::Done) {
        switch (stage) {
        case Stage::TtHint:
            stage = Stage::LoadPrimary;
            if (!tt_hint.is_null())
                return tt_hint;
            break;
        case Stage::Evasions:
            if (Move move = candidates.take_evasion(); !move.is_null())
                return move;
            stage = Stage::Done;
            break;
        case Stage::LoadPrimary:
            if (in_check) {
                candidates.load_evasions(board, history, side, tt_hint);
                stage = Stage::Evasions;
                break;
            }
            candidates.load_noisy<MovePolicy::Quiescence>(board, history, tt_hint);
            stage = Stage::Noisy;
            [[fallthrough]];
        case Stage::Noisy: {
            const Move move = candidates.take_noisy(
                [this](Move move, int) { return move.type() == MOVE_PROM || board.see(move, 0); });
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
