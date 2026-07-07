#pragma once

#include <array>
#include <cstdint>

#include "core/move.hpp"
#include "movegen/move_list.hpp"
#include "search/move_ordering.hpp"

class Board;

class MovePicker {
public:
    static MovePicker main_search(const Board&                 board,
                                  const MoveOrdering&          ordering,
                                  const MoveOrdering::Context& context,
                                  int                          ply,
                                  Move                         tt_move = NULL_MOVE);

    static MovePicker qsearch(const Board&                 board,
                              const MoveOrdering&          ordering,
                              const MoveOrdering::Context& context,
                              Move                         tt_move = NULL_MOVE);

    MovePicker(const MovePicker&)            = delete;
    MovePicker(MovePicker&&)                 = delete;
    MovePicker& operator=(const MovePicker&) = delete;
    MovePicker& operator=(MovePicker&&)      = delete;

    // Returns ordered pseudo-legal candidates; search remains the legal-move authority.
    Move next();
    void skip_quiet_moves();

private:
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
        KILLER_1,
        KILLER_2,
        COUNTERMOVE,
        LOAD_QUIET,
        PICK_QUIET,
        PICK_BAD_NOISY,
        DONE,
    };

    enum class ScoreKind : uint8_t {
        Noisy,
        Quiet,
        Evasion,
    };

    enum class PickKind : uint8_t {
        Evasion,
        GoodNoisy,
        Quiet,
        BadNoisy,
    };

    struct ScoredBand {
        ScoredMove* cur{nullptr};
        ScoredMove* end{nullptr};
    };

    struct QuietHints {
        Move killer_1{NULL_MOVE};
        Move killer_2{NULL_MOVE};
        Move counter{NULL_MOVE};

        bool contains(Move move) const;
    };

    MovePicker(const Board&                 board,
               const MoveOrdering&          ordering,
               const MoveOrdering::Context& context,
               Mode                         mode,
               Move                         tt,
               QuietHints                   hints);

    void set_hints(QuietHints hints);
    bool accepts_quiet_hints() const;
    bool matches_tt(Move move) const;
    bool matches_quiet_hint(Move move) const;
    Move accepted_tt_hint(Move move) const;
    Move accepted_quiet_hint(Move move, QuietHints used_hints) const;

    template <ScoreKind Kind>
    MoveScore score_move(Move move) const;
    MoveScore score_noisy(Move move) const;

    template <ScoreKind Kind>
    ScoredMove* score_moves(const MoveList& list, ScoredMove* out);

    template <PickKind Kind>
    bool is_pickable(const ScoredMove& move) const;
    template <PickKind Kind>
    Move pick(ScoredBand& band);

    const Board&                      board;
    const MoveOrdering&               ordering;
    const MoveOrdering::Context       context;
    Move                              tt_move{NULL_MOVE};
    const Mode                        mode;
    const bool                        in_check;
    Stage                             stage{Stage::TT_MOVE};
    std::array<ScoredMove, MoveList::capacity> moves;
    // Generated holds noisy moves or evasions; quiets are appended after it.
    ScoredBand generated;
    ScoredBand quiet;
    Move       killer_1{NULL_MOVE};
    Move       killer_2{NULL_MOVE};
    Move       counter{NULL_MOVE};
    bool       skip_quiets{false};
};
