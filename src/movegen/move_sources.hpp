#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "core/move.hpp"

class Board;

namespace search {
class History;
struct OrderingContext;
} // namespace search

namespace movegen {

// Ordering and eligibility of a legal move stream.
enum class MovePolicy { Unscored, MainSearch, Quiescence };

template <MovePolicy Policy>
class Moves;

template <MovePolicy Policy>
class MoveSource;

// Candidate storage and selection.

inline constexpr std::size_t MoveCapacity = 256;

// Owns complete scored batches and their selection. Sources control stage progression,
// validate hints, and supply SEE acceptance and quiet eligibility.
// The templated operations are defined and instantiated in move_sources.cpp.
class OrderedCandidates {
public:
    OrderedCandidates() = default;

    // Active ranges point into this object's storage.
    OrderedCandidates(const OrderedCandidates&) = delete;
    OrderedCandidates(OrderedCandidates&&) = delete;
    OrderedCandidates& operator=(const OrderedCandidates&) = delete;
    OrderedCandidates& operator=(OrderedCandidates&&) = delete;

    void load_evasions(const Board& board,
                       const search::History& history,
                       Color side,
                       Move tt_hint);

    template <MovePolicy Policy>
    void load_noisy(const Board& board, const search::History& history, Move tt_hint);

    void load_quiets(const Board& board,
                     const search::History& history,
                     const search::OrderingContext& context,
                     Move tt_hint,
                     std::span<const Move> quiet_hints);

    Move take_evasion();

    // Acceptance receives (move, cached SEE threshold) after ranking.
    template <typename Accept>
    Move take_noisy(Accept accept);

    // Eligibility receives a move during ranking, before selection is final.
    template <typename Eligible>
    Move take_quiet(Eligible eligible);

    Move take_deferred_noisy();

private:
    enum class CandidateState : std::uint8_t { Ready, Excluded, Deferred };

    struct Candidate {
        // Scoring fills every field before the candidate becomes part of an active range.
        // Unused slots remain uninitialized.
        Move move;
        CandidateState state;
        int score;
        // Captured with the score, before child searches can train the history table.
        int see_threshold;
    };

    struct Range {
        Candidate* cursor{nullptr};
        Candidate* end{nullptr};
    };

    template <typename Eligible>
    static Candidate* find_best_ready(const Range& range, Eligible eligible);

    static Move consume(Range& range, Candidate* selected);

    std::array<Candidate, MoveCapacity> storage;
    // Evasions when in check, otherwise noisy moves; quiets load only if reached.
    Range primary;
    Range quiet;
};

// Move sources for each policy.

// Unscored traversal loads the complete batch before consuming it in generation order.
template <>
class MoveSource<MovePolicy::Unscored> {
    friend class Moves<MovePolicy::Unscored>;

    explicit MoveSource(const Board& board);

    // Traversal cursors point into this object's storage.
    MoveSource(const MoveSource&) = delete;
    MoveSource(MoveSource&&) = delete;
    MoveSource& operator=(const MoveSource&) = delete;
    MoveSource& operator=(MoveSource&&) = delete;

    Move next_candidate() { return cursor != end ? *cursor++ : NULL_MOVE; }

    const Board& board;
    std::array<Move, MoveCapacity> storage;
    Move* cursor;
    Move* end;
};

// Hints are node-start snapshots; scores freeze when their batch loads.
// Main search owns quiet hints and suppression; quiescence has no quiet stage.
template <>
class MoveSource<MovePolicy::MainSearch> {
    friend class Moves<MovePolicy::MainSearch>;

    MoveSource(const Board& board,
               const search::History& history,
               const search::OrderingContext& context);
    // The source borrows context. Reject temporaries that would be destroyed
    // at the end of the construction expression, before traversal begins.
    MoveSource(const Board&, const search::History&, const search::OrderingContext&&) = delete;
    Move next_candidate();
    void skip_nonchecking_quiets();

    enum class Stage : std::uint8_t {
        TtHint,
        LoadPrimary,
        Evasions,
        EarlyNoisy,
        QuietHints,
        Quiets,
        DeferredNoisy,
        Done,
    };
    static constexpr int QuietHintCapacity = 3;

    const Board& board;
    const search::History& history;
    const search::OrderingContext& context;
    Move tt_hint{NULL_MOVE};
    Stage stage{Stage::TtHint};
    OrderedCandidates candidates;
    std::array<Move, QuietHintCapacity> quiet_hints{};
    int quiet_hint_count{0};
    int quiet_hint_index{0};
    bool quiets_must_check{false};
};

// Checked nodes return all legal evasions. Outside check, the TT hint and promotions
// bypass SEE; other captures must pass SEE at zero, with history used for ordering.
template <>
class MoveSource<MovePolicy::Quiescence> {
    friend class Moves<MovePolicy::Quiescence>;

    MoveSource(const Board& board, const search::History& history, Move tt_move = NULL_MOVE);

    Move next_candidate();

    enum class Stage : std::uint8_t { TtHint, LoadPrimary, Evasions, Noisy, Done };

    const Board& board;
    const search::History& history;
    const Color side;
    Move tt_hint{NULL_MOVE};
    const bool in_check;
    Stage stage{Stage::TtHint};
    OrderedCandidates candidates;
};

} // namespace movegen
