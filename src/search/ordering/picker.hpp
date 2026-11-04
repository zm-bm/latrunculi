#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <utility>

#include "board/board.hpp"
#include "core/move.hpp"
#include "movegen/move_list.hpp"
#include "search/ordering/history.hpp"

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
    Move take_best(Eligible eligible) {
        Candidate* selected = best<Which, State>(eligible);
        return selected ? take<Which>(selected) : NULL_MOVE;
    }

    template <Batch Which, CandidateState State = CandidateState::Ready>
    Move take_best() {
        return take_best<Which, State>([](const Candidate&) { return true; });
    }

    // Test acceptance only after ranking. Rejected candidates stay in place for later.
    template <typename Accept>
    Move take_best_or_defer(Accept accept) {
        while (Candidate* selected = best<Batch::Primary>()) {
            if (accept(*selected))
                return take<Batch::Primary>(selected);
            defer(*selected);
        }
        return NULL_MOVE;
    }

private:
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

// Main search owns quiet hints and suppression; quiescence has no quiet stage.
class MainPicker {
public:
    MainPicker(const Board&       board,
               const History&     history,
               const NodeContext& context,
               int                ply,
               Move               tt_move = NULL_MOVE);
    MainPicker(const MainPicker&)            = delete;
    MainPicker(MainPicker&&)                 = delete;
    MainPicker& operator=(const MainPicker&) = delete;
    MainPicker& operator=(MainPicker&&)      = delete;

    // Restore this node before each call. Legality follows selection so swaps and ties
    // survive filtering. Hints are node-start snapshots; scores freeze at batch loading.
    Move next() {
        for (Move move = next_candidate(); !move.is_null(); move = next_candidate()) {
            if (board.is_legal_pseudo_move(move))
                return move;
        }
        return NULL_MOVE;
    }
    void skip_nonchecking_quiets();
    // Raw node-start snapshot, even if excluded from the validated quiet hints.
    Move counter_hint() const { return counter_move; }

private:
    enum class Stage : std::uint8_t {
        TtMove,
        LoadPrimary,
        PickEvasion,
        PickEarlyNoisy,
        PickQuietHint,
        LoadQuiet,
        PickQuiet,
        PickDeferredNoisy,
        Done,
    };
    static constexpr int QuietHintCapacity = 3;
    struct QuietHintCandidates {
        Move primary_killer;
        Move secondary_killer;
        Move counter;
    };
    MainPicker(const Board&        board,
               const History&      history,
               const NodeContext&  context,
               Move                tt,
               QuietHintCandidates quiet_hint_candidates);
    Move next_candidate();
    bool is_quiet_hint(Move move) const;

    const Board&                        board;
    const History&                      history;
    const NodeContext                   context;
    const Move                          counter_move;
    Move                                tt_move{NULL_MOVE};
    const bool                          in_check;
    Stage                               stage{Stage::TtMove};
    detail::CandidateBuffer             candidates;
    std::array<Move, QuietHintCapacity> quiet_hints{};
    int                                 quiet_hint_count{0};
    int                                 quiet_hint_next{0};
    bool                                quiet_checks_only{false};
};

class QuiescencePicker {
public:
    QuiescencePicker(const Board& board, const History& history, Move tt_move = NULL_MOVE);
    QuiescencePicker(const QuiescencePicker&)            = delete;
    QuiescencePicker(QuiescencePicker&&)                 = delete;
    QuiescencePicker& operator=(const QuiescencePicker&) = delete;
    QuiescencePicker& operator=(QuiescencePicker&&)      = delete;

    // Checked nodes return all legal evasions; ordinary nodes keep non-losing noisy
    // moves. Capture history orders them, while the exchange threshold stays zero.
    Move next() {
        for (Move move = next_candidate(); !move.is_null(); move = next_candidate()) {
            if (board.is_legal_pseudo_move(move))
                return move;
        }
        return NULL_MOVE;
    }

private:
    enum class Stage : std::uint8_t { TtMove, LoadPrimary, PickEvasion, PickNoisy, Done };
    Move next_candidate();

    const Board&            board;
    const History&          history;
    const NodeContext       context;
    Move                    tt_move{NULL_MOVE};
    const bool              in_check;
    Stage                   stage{Stage::TtMove};
    detail::CandidateBuffer candidates;
};

} // namespace search::ordering
