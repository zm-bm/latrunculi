#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <utility>

#include "board/board.hpp"
#include "core/move.hpp"
#include "movegen/generator.hpp"
#include "search/ordering/history.hpp"

namespace movegen::detail {

using search::ordering::History;
using search::ordering::NodeContext;

enum class CandidateState : std::uint8_t { Ready, Excluded, Deferred };
enum class Batch : std::uint8_t { Primary, Quiet };

struct Candidate {
    // Scoring fills every field before the candidate becomes part of an active range.
    // Unused slots remain uninitialized.
    Move           move;
    CandidateState state;
    int            score;
    // Captured with the score, before child searches can train the history table.
    int see_threshold;
};

// Selection mechanics only; ordered sources supply scored and classified candidates.
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
        const Move move = range.next->move;
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

    std::array<Candidate, MoveCapacity> storage;
    // Evasions when in check, otherwise noisy moves; quiets load only if reached.
    Range primary;
    Range quiet;
};

// Hints are node-start snapshots; scores freeze when their batch loads.
// Main search owns quiet hints and suppression; quiescence has no quiet stage.
template <>
class MoveSource<Mode::MainSearch> {
    friend class Moves<Mode::MainSearch>;

    MoveSource(const Board&       board,
               const History&     history,
               const NodeContext& context,
               int                ply,
               Move               tt_move = NULL_MOVE);
    void skip_nonchecking_quiets();

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
    MoveSource(const Board&        board,
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
    CandidateBuffer                     candidates;
    std::array<Move, QuietHintCapacity> quiet_hints{};
    int                                 quiet_hint_count{0};
    int                                 quiet_hint_next{0};
    bool                                quiet_checks_only{false};
};

// Checked nodes return all legal evasions; ordinary nodes keep non-losing noisy
// moves. Capture history orders them, while the exchange threshold stays zero.
template <>
class MoveSource<Mode::Quiescence> {
    friend class Moves<Mode::Quiescence>;

    MoveSource(const Board& board, const History& history, Move tt_move = NULL_MOVE);

    enum class Stage : std::uint8_t { TtMove, LoadPrimary, PickEvasion, PickNoisy, Done };
    Move next_candidate();

    const Board&      board;
    const History&    history;
    const NodeContext context;
    Move              tt_move{NULL_MOVE};
    const bool        in_check;
    Stage             stage{Stage::TtMove};
    CandidateBuffer   candidates;
};

} // namespace movegen::detail
