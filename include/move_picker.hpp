#pragma once

#include <array>
#include <cstdint>

#include "move.hpp"

class Board;
class KillerMoves;
class MoveList;
struct QuietHistory;

class MovePicker {
public:
    static MovePicker main_search(const Board&        board,
                                  const KillerMoves&  killers,
                                  const QuietHistory& history,
                                  int                 ply,
                                  Move                tt_move = NULL_MOVE);

    static MovePicker
    qsearch(const Board& board, const QuietHistory& history, Move tt_move = NULL_MOVE);

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

    MovePicker(const Board&        board,
               const QuietHistory& history,
               Mode                mode,
               Move                tt_move,
               Move                killer_1,
               Move                killer_2);

    bool is_tt_duplicate(Move move) const;
    bool is_quiet_hint_duplicate(Move move) const;
    bool is_valid_hint(Move move) const;
    bool is_valid_tt_hint(Move move) const;
    bool is_returnable_killer(Move move, Move previous_killer) const;

    template <ScoreKind Kind>
    MoveScore score_move(Move move) const;
    template <ScoreKind Kind>
    ScoredMove* score_moves(const MoveList& list, ScoredMove* out);

    template <PickKind Kind>
    bool is_pickable(const ScoredMove& move) const;
    template <PickKind Kind>
    Move pick(ScoredBand& band);

    const Board&                      board;
    const QuietHistory&               history;
    Move                              tt_move{NULL_MOVE};
    const Mode                        mode;
    const bool                        in_check;
    const Color                       side;
    Stage                             stage{Stage::TT_MOVE};
    std::array<ScoredMove, MAX_MOVES> moves;
    // Generated holds noisy moves or evasions; quiets are appended after it.
    ScoredBand generated;
    ScoredBand quiet;
    Move       killer_1{NULL_MOVE};
    Move       killer_2{NULL_MOVE};
    bool       skip_quiets{false};
};
