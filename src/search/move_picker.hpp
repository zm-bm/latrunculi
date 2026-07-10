#pragma once

#include <array>
#include <cstdint>

#include "core/move.hpp"
#include "movegen/move_list.hpp"
#include "search/move_ordering.hpp"

class Board;

namespace move_picker {

enum class Mode : uint8_t {
    MainSearch,
    QSearch,
};

enum class Stage : uint8_t {
    TT_MOVE,
    LOAD_EVASIONS,
    PICK_EVASION,
    LOAD_NOISY,
    PICK_GOOD_NOISY,
    PICK_QUIET_HINT,
    LOAD_QUIET,
    PICK_QUIET,
    PICK_BAD_NOISY,
    DONE,
};

enum class ScorePolicy : uint8_t {
    Noisy,
    Quiet,
    Evasion,
};

enum class PickPolicy : uint8_t {
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

class Picker {
public:
    Picker(const Picker&)            = delete;
    Picker(Picker&&)                 = delete;
    Picker& operator=(const Picker&) = delete;
    Picker& operator=(Picker&&)      = delete;

    // Returns ordered pseudo-legal candidates; search remains the legal-move authority.
    Move next();
    void skip_quiet_moves();

private:
    friend Picker main_search(const Board&                 board,
                              const MoveOrdering&          ordering,
                              const MoveOrdering::Context& context,
                              int                          ply,
                              Move                         tt_move);

    friend Picker qsearch(const Board&                 board,
                          const MoveOrdering&          ordering,
                          const MoveOrdering::Context& context,
                          Move                         tt_move);

    static constexpr int QuietHintCapacity = 3;
    using QuietHintCandidates              = std::array<Move, QuietHintCapacity>;

    Picker(Mode                         mode,
           const Board&                 board,
           const MoveOrdering&          ordering,
           const MoveOrdering::Context& context,
           Move                         tt,
           QuietHintCandidates          quiet_hint_candidates = {});

    void add_quiet_hint(Move move);
    bool matches_tt(Move move) const;
    bool matches_quiet_hint(Move move) const;
    Move accepted_tt_hint(Move move) const;
    Move accepted_quiet_hint(Move move) const;
    Move next_quiet_hint();

    template <ScorePolicy Policy>
    int score_move(Move move) const;
    int score_noisy(Move move) const;

    template <ScorePolicy Policy>
    Candidate* score_moves(const MoveList& list, Candidate* out);

    template <PickPolicy Policy>
    bool is_pickable(const Candidate& candidate) const;
    template <PickPolicy Policy>
    Move pick(CandidateRange& range);

    const Board&                              board;
    const MoveOrdering&                       ordering;
    const MoveOrdering::Context               context;
    Move                                      tt_move{NULL_MOVE};
    const Mode                                mode;
    const bool                                in_check;
    Stage                                     stage{Stage::TT_MOVE};
    std::array<Candidate, MoveList::capacity> moves;
    // Primary holds evasions when in check, otherwise noisy moves.
    CandidateRange                      primary;
    CandidateRange                      quiets;
    std::array<Move, QuietHintCapacity> quiet_hints{};
    int                                 quiet_hint_count{0};
    int                                 quiet_hint_next{0};
    bool                                skip_quiets{false};
};

Picker main_search(const Board&                 board,
                   const MoveOrdering&          ordering,
                   const MoveOrdering::Context& context,
                   int                          ply,
                   Move                         tt_move = NULL_MOVE);

Picker qsearch(const Board&                 board,
               const MoveOrdering&          ordering,
               const MoveOrdering::Context& context,
               Move                         tt_move = NULL_MOVE);

} // namespace move_picker
