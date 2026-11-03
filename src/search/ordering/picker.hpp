#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <utility>

#include "board/board.hpp"
#include "core/move.hpp"
#include "movegen/move_list.hpp"
#include "search/ordering/history.hpp"

namespace movegen {
enum class MoveGenType;
}

namespace search::ordering {

namespace detail {

enum class CandidateState : std::uint8_t { Ready, Excluded, Deferred };
enum class Batch : std::uint8_t { Primary, Quiet };

struct Candidate {
    // Packed storage avoids initializing unused slots. Scoring fills every field
    // before the candidate becomes part of an active range.
    MoveBits       move_bits;
    CandidateState state;
    int            score;
    // Captured with the score, before child searches can train the history table.
    int see_threshold;

    Move move() const {
        Move result;
        result.bits = move_bits;
        return result;
    }
};

// Selection mechanics only; Picker supplies scored and classified candidates.
class CandidateBuffer {
public:
    template <Batch Which, typename Populate>
    void load(Populate&& populate) {
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

    template <Batch Which, CandidateState State = CandidateState::Ready, typename Eligible>
    Candidate* best(Eligible eligible) {
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

    template <Batch Which, CandidateState State = CandidateState::Ready>
    Candidate* best() {
        return best<Which, State>([](const Candidate&) { return true; });
    }

    template <Batch Which>
    Move take(Candidate* best) {
        auto& range = range_for<Which>();
        // Retain excluded entries: removing them changes later selection ties.
        std::swap(*range.next, *best);
        const Move move = range.next->move();
        ++range.next;
        return move;
    }

    void defer(Candidate& candidate) {
        // Defer in place, without swapping or advancing. Deferred scores tie at zero.
        candidate.state = CandidateState::Deferred;
        candidate.score = 0;
    }

private:
    struct Range {
        Candidate* next{nullptr};
        Candidate* end{nullptr};
    };

    template <Batch Which>
    Range& range_for() {
        if constexpr (Which == Batch::Primary)
            return primary;
        else
            return quiet;
    }

    std::array<Candidate, movegen::MoveList::capacity> storage;
    // Evasions when in check, otherwise noisy moves; quiets load only if reached.
    Range primary;
    Range quiet;
};

} // namespace detail

// Search mode is fixed by the picker type; both modes retain legal check evasions.
enum class PickerMode : std::uint8_t { MainSearch, Quiescence };

template <PickerMode Mode>
class Picker {
public:
    Picker(const Picker&)            = delete;
    Picker(Picker&&)                 = delete;
    Picker& operator=(const Picker&) = delete;
    Picker& operator=(Picker&&)      = delete;

    Picker(const Board&       board,
           const History&     history,
           const NodeContext& context,
           int                ply,
           Move               tt_move = NULL_MOVE) requires(Mode == PickerMode::MainSearch);

    Picker(const Board& board, const History& history, Move tt_move = NULL_MOVE)
        requires(Mode == PickerMode::Quiescence);

    // Returns ordered legal moves, filtering after selection to preserve swaps and ties.
    // Restore the board to this node before each call. Hints are node-start snapshots;
    // scores and SEE thresholds are captured when each stage loads, including lazy quiets.
    Move next() {
        for (Move move = next_candidate(); !move.is_null(); move = next_candidate()) {
            if (board.is_legal_pseudo_move(move))
                return move;
        }
        return NULL_MOVE;
    }
    void skip_nonchecking_quiets() requires(Mode == PickerMode::MainSearch);

    // Raw node-start snapshot, even if validation excludes it from the quiet hints.
    Move counter_hint() const requires(Mode == PickerMode::MainSearch)
    {
        return counter_move;
    }

private:
    enum class Stage : std::uint8_t {
        TtMove,
        LoadEvasions,
        PickEvasion,
        LoadNoisy,
        PickEarlyNoisy,
        PickQuietHint,
        LoadQuiet,
        PickQuiet,
        PickDeferredNoisy,
        Done,
    };

    using CandidateState  = detail::CandidateState;
    using Batch           = detail::Batch;
    using Candidate       = detail::Candidate;
    using CandidateBuffer = detail::CandidateBuffer;

    static constexpr int QuietHintCapacity = 3;
    struct QuietHintCandidates {
        Move primary_killer;
        Move secondary_killer;
        Move counter;
    };

    Picker(const Board&        board,
           const History&      history,
           const NodeContext&  context,
           Move                tt,
           QuietHintCandidates quiet_hint_candidates = {});

    Move next_candidate();

    bool is_quiet_hint(Move move) const;
    Move validate_tt_hint(Move move) const;

    template <movegen::MoveGenType Kind>
    void load();

    template <movegen::MoveGenType Kind>
    Candidate score_move(Move move) const;

    const Board&                        board;
    const History&                      history;
    const NodeContext                   context;
    const Move                          counter_move;
    Move                                tt_move{NULL_MOVE};
    const bool                          in_check;
    Stage                               stage{Stage::TtMove};
    CandidateBuffer                     candidates;
    std::array<Move, QuietHintCapacity> quiet_hints{};
    int                                 quiet_hint_count{0};
    int                                 quiet_hint_next{0};
    bool                                quiet_checks_only{false};
};

using MainPicker       = Picker<PickerMode::MainSearch>;
using QuiescencePicker = Picker<PickerMode::Quiescence>;

extern template class Picker<PickerMode::MainSearch>;
extern template class Picker<PickerMode::Quiescence>;

} // namespace search::ordering
