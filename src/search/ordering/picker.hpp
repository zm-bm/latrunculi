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
    Move next();
    void skip_quiet_moves();

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
        PickGoodNoisy,
        PickQuietHint,
        LoadQuiet,
        PickQuiet,
        PickBadNoisy,
        Done,
    };

    enum class ScorePolicy : std::uint8_t {
        Noisy,
        Quiet,
        Evasion,
    };

    enum class PickPolicy : std::uint8_t {
        Evasion,
        GoodNoisy,
        Quiet,
        BadNoisy,
    };

    struct Candidate {
        Move move;
        int  score;
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
    bool is_tt_move(Move move) const;
    bool is_quiet_hint(Move move) const;
    Move validate_tt_hint(Move move) const;
    Move validate_quiet_hint(Move move) const;
    Move next_quiet_hint();

    template <ScorePolicy Policy>
    int score_move(Move move) const;
    int score_noisy(Move move) const;

    template <ScorePolicy Policy>
    Candidate* score_moves(const movegen::MoveList& list, Candidate* out);

    template <PickPolicy Policy>
    bool is_pickable(const Candidate& candidate) const;
    template <PickPolicy Policy>
    Move pick(CandidateRange& range);

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
    bool                                skip_quiets{false};
};

} // namespace search::ordering
