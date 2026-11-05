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

// Scoring arithmetic.

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

} // namespace

// Batch loading.

// Each loader scores even excluded entries and publishes only after generation finishes.
void OrderedCandidates::load_evasions(const Board&   board,
                                      const History& history,
                                      Color          side,
                                      Move           tt_move) {
    Candidate* const first = storage.data();
    Candidate*       end   = first;
    generate<GenerationKind::Evasions>(board, [&, side](Move move, PieceType moving_piece) {
        Candidate candidate{
            .move = move, .state = CandidateState::Ready, .score = 0, .see_threshold = 0};
        if (move.type() == MOVE_PROM) {
            candidate.score = PromotionScore;
        } else if (board.is_capture(move)) {
            const int victim   = see::piece_value(board.captured_piece_type(move));
            const int attacker = see::piece_value(moving_piece);
            candidate.score    = EvasionCaptureScoreBase + capture_material_score(victim, attacker);
        } else {
            candidate.score = history.base_quiet_score(side, move);
        }
        if (move == tt_move)
            candidate.state = CandidateState::Excluded;
        assert(end < storage.data() + storage.size());
        *end++ = candidate;
    });
    primary = {first, end};
}

template <MovePolicy Policy>
void OrderedCandidates::load_noisy(const Board& board, const History& history, Move tt_move) {
    static_assert(Policy == MovePolicy::MainSearch || Policy == MovePolicy::Quiescence);
    Candidate* const first = storage.data();
    Candidate*       end   = first;
    generate<GenerationKind::Noisy>(board, [&](Move move, PieceType moving_piece) {
        Candidate candidate{
            .move = move, .state = CandidateState::Ready, .score = 0, .see_threshold = 0};
        if (move.type() == MOVE_PROM) {
            candidate.score = PromotionScore;
        } else {
            const PieceType victim         = board.captured_piece_type(move);
            const int       victim_value   = see::piece_value(victim);
            const int       attacker_value = see::piece_value(moving_piece);
            const int       raw_history    = history.capture_history_score(
                board.side_to_move(), moving_piece, move.to(), victim);
            const int scaled_history = raw_history / CaptureHistoryDivisor;
            candidate.score          = capture_score(victim_value, attacker_value, scaled_history);
            if constexpr (Policy == MovePolicy::MainSearch)
                candidate.see_threshold = main_search_see_threshold(victim, scaled_history);
        }
        if (move == tt_move)
            candidate.state = CandidateState::Excluded;
        assert(end < storage.data() + storage.size());
        *end++ = candidate;
    });
    primary = {first, end};
}

void OrderedCandidates::load_quiets(const Board&           board,
                                    const History&         history,
                                    const OrderingContext& context,
                                    Move                   tt_move,
                                    std::span<const Move>  quiet_hints) {
    // Append after the full primary batch, including taken and excluded entries.
    assert(primary.end != nullptr);
    Candidate* const first = primary.end;
    Candidate*       end   = first;
    generate<GenerationKind::Quiet>(board, [&](Move move, PieceType moving_piece) {
        Candidate candidate{.move          = move,
                            .state         = CandidateState::Ready,
                            .score         = history.quiet_score(context, moving_piece, move),
                            .see_threshold = 0};
        bool      excluded = move == tt_move;
        if (!excluded) {
            for (Move hint : quiet_hints) {
                if (move == hint) {
                    excluded = true;
                    break;
                }
            }
        }
        if (excluded)
            candidate.state = CandidateState::Excluded;
        assert(end < storage.data() + storage.size());
        *end++ = candidate;
    });
    quiet = {first, end};
}

// Candidate storage and selection.

inline Move OrderedCandidates::take_evasion() {
    return take_best<Batch::Primary>();
}

// Test acceptance only after ranking. Rejected candidates stay in place for later.
template <typename Accept>
inline Move OrderedCandidates::take_noisy(Accept accept) {
    while (Candidate* selected = best<Batch::Primary>()) {
        if (accept(selected->move, selected->see_threshold))
            return take<Batch::Primary>(selected);
        defer(*selected);
    }
    return NULL_MOVE;
}

template <typename Eligible>
inline Move OrderedCandidates::take_quiet(Eligible eligible) {
    return take_best<Batch::Quiet>(
        [&](const Candidate& candidate) { return eligible(candidate.move); });
}

inline Move OrderedCandidates::take_deferred_noisy() {
    return take_best<Batch::Primary, CandidateState::Deferred>();
}

template <OrderedCandidates::Batch          Which,
          OrderedCandidates::CandidateState State,
          typename Eligible>
inline Move OrderedCandidates::take_best(Eligible eligible) {
    Candidate* selected = best<Which, State>(eligible);
    return selected ? take<Which>(selected) : NULL_MOVE;
}

template <OrderedCandidates::Batch Which, OrderedCandidates::CandidateState State>
inline Move OrderedCandidates::take_best() {
    return take_best<Which, State>([](const Candidate&) { return true; });
}

template <OrderedCandidates::Batch Which>
inline OrderedCandidates::Range& OrderedCandidates::range_for() {
    if constexpr (Which == Batch::Primary)
        return primary;
    else
        return quiet;
}

template <OrderedCandidates::Batch          Which,
          OrderedCandidates::CandidateState State,
          typename Eligible>
inline OrderedCandidates::Candidate* OrderedCandidates::best(Eligible eligible) {
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

template <OrderedCandidates::Batch Which, OrderedCandidates::CandidateState State>
inline OrderedCandidates::Candidate* OrderedCandidates::best() {
    return best<Which, State>([](const Candidate&) { return true; });
}

template <OrderedCandidates::Batch Which>
inline Move OrderedCandidates::take(Candidate* best) {
    auto& range = range_for<Which>();
    // Retain excluded entries: removing them changes later selection ties.
    std::swap(*range.next, *best);
    const Move move = range.next->move;
    ++range.next;
    return move;
}

inline void OrderedCandidates::defer(Candidate& candidate) {
    // Defer in place, without swapping or advancing. Deferred scores tie at zero.
    candidate.state = CandidateState::Deferred;
    candidate.score = 0;
}

namespace {

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

Move MoveSource<MovePolicy::MainSearch>::next_candidate() {
    while (stage != Stage::Done) {
        switch (stage) {
        case Stage::TtMove:
            stage = Stage::LoadPrimary;
            if (!tt_move.is_null())
                return tt_move;
            break;
        case Stage::PickEvasion:
            if (Move move = candidates.take_evasion(); !move.is_null())
                return move;
            stage = Stage::Done;
            break;
        case Stage::LoadPrimary:
            if (context.in_check) {
                candidates.load_evasions(board, history, context.side, tt_move);
                stage = Stage::PickEvasion;
                break;
            }
            candidates.load_noisy<MovePolicy::MainSearch>(board, history, tt_move);
            stage = Stage::PickEarlyNoisy;
            [[fallthrough]];
        case Stage::PickEarlyNoisy: {
            const Move move = candidates.take_noisy([this](Move move, int see_threshold) {
                return move.type() == MOVE_PROM || board.see(move, see_threshold);
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
            candidates.load_quiets(
                board,
                history,
                context,
                tt_move,
                {quiet_hints.data(), static_cast<std::size_t>(quiet_hint_count)});
            stage = Stage::PickQuiet;
            [[fallthrough]];
        case Stage::PickQuiet: {
            const Move move = candidates.take_quiet(
                [this](Move move) { return !quiet_checks_only || board.gives_check(move); });
            if (!move.is_null())
                return move;
            stage = Stage::PickDeferredNoisy;
            break;
        }
        case Stage::PickDeferredNoisy:
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
        quiet_checks_only = true;
}

bool MoveSource<MovePolicy::MainSearch>::is_quiet_hint(Move move) const {
    for (int i = 0; i < quiet_hint_count; ++i)
        if (quiet_hints[i] == move)
            return true;
    return false;
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
            if (Move move = candidates.take_evasion(); !move.is_null())
                return move;
            stage = Stage::Done;
            break;
        case Stage::LoadPrimary:
            if (in_check) {
                candidates.load_evasions(board, history, side, tt_move);
                stage = Stage::PickEvasion;
                break;
            }
            candidates.load_noisy<MovePolicy::Quiescence>(board, history, tt_move);
            stage = Stage::PickNoisy;
            [[fallthrough]];
        case Stage::PickNoisy: {
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
