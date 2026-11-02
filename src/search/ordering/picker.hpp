#pragma once

#include <array>
#include <cstdint>

#include "core/move.hpp"
#include "movegen/move_list.hpp"
#include "search/ordering/state.hpp"

class Board;

namespace search::ordering {

class Picker {
public:
    Picker(const Picker&)            = delete;
    Picker(Picker&&)                 = delete;
    Picker& operator=(const Picker&) = delete;
    Picker& operator=(Picker&&)      = delete;

    static Picker for_main_search(const Board&          board,
                                  const State&          state,
                                  const State::Context& context,
                                  int                   ply,
                                  Move                  tt_move = NULL_MOVE);

    static Picker for_quiescence(const Board& board, const State& state, Move tt_move = NULL_MOVE);

    // Returns ordered pseudo-legal candidates; search remains the legal-move authority.
    // Restore the board to this node before each call. Histories may change between
    // calls; generated lists retain their scoring-time snapshot.
    Move next();
    void skip_nonchecking_quiets();

private:
    enum class Mode : std::uint8_t {
        MainSearch,
        QSearch,
    };

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

    enum class ScorePolicy : std::uint8_t {
        Noisy,
        Quiet,
        Evasion,
    };

    struct Candidate {
        Move move;
        bool deferred;
        int  score;
        // Captured with the score, before child searches can train the history table.
        int see_threshold;
    };

    struct CandidateRange {
        Candidate* next{nullptr};
        Candidate* end{nullptr};
    };

    static constexpr int QuietHintCapacity = 3;
    using QuietHintCandidates              = std::array<Move, QuietHintCapacity>;

    Picker(Mode                  mode,
           const Board&          board,
           const State&          state,
           const State::Context& context,
           Move                  tt,
           QuietHintCandidates   quiet_hint_candidates = {});

    void add_quiet_hint(Move move);
    bool is_quiet_hint(Move move) const;
    Move validate_tt_hint(Move move) const;
    Move validate_quiet_hint(Move move) const;

    template <ScorePolicy Policy>
    Candidate score_move(Move move) const;

    template <ScorePolicy Policy>
    Candidate* score_moves(const movegen::MoveList& list, Candidate* out);

    template <typename Eligible>
    static Candidate* find_best(CandidateRange& range, Eligible eligible);
    static Move       take(CandidateRange& range, Candidate* best);

    const Board&                                       board;
    const State&                                       state;
    const State::Context                               context;
    Move                                               tt_move{NULL_MOVE};
    const Mode                                         mode;
    const bool                                         in_check;
    Stage                                              stage{Stage::TtMove};
    std::array<Candidate, movegen::MoveList::capacity> candidates;
    // Holds evasions when in check, otherwise noisy moves.
    CandidateRange                      primary_range;
    CandidateRange                      quiet_range;
    std::array<Move, QuietHintCapacity> quiet_hints{};
    int                                 quiet_hint_count{0};
    int                                 quiet_hint_next{0};
    bool                                quiet_checks_only{false};
};

} // namespace search::ordering
