#pragma once

#include <array>
#include <cstdint>

#include "move.hpp"

class Board;
struct HistoryTable;
struct KillerMoves;

struct MovePickerContext {
    const Board&        board;
    const KillerMoves&  killers;
    const HistoryTable& history;
    int                 ply;
    Move                pv_move;
    Move                tt_move;
};

enum class MovePickerMode : uint8_t {
    MainSearch,
    QSearch,
};

enum class MovePickSource : uint8_t {
    None,
    PV,
    Hash,
    Evasion,
    GoodNoisy,
    Killer,
    Quiet,
    BadNoisy,
};

class MovePicker {
public:
    MovePicker(const MovePickerContext& ctx, MovePickerMode mode);
    MovePicker(const MovePicker&)            = delete;
    MovePicker(MovePicker&&)                 = delete;
    MovePicker& operator=(const MovePicker&) = delete;
    MovePicker& operator=(MovePicker&&)      = delete;

    Move           next();
    void           skip_quiet_moves();
    MoveScore      last_score() const { return last_picked_score; }
    MovePickSource last_source() const { return last_picked_source; }
    bool           noisy_generated() const { return noisy_ready; }
    bool           quiet_generated() const { return quiet_ready; }
    bool           tracks_staged_generation() const {
        return mode == MovePickerMode::MainSearch && !in_check;
    }

private:
    enum class Stage : uint8_t {
        PV,
        HASH,
        GENERATE_EVASIONS,
        EVASION,
        GENERATE_NOISY,
        GOOD_NOISY,
        KILLER_1,
        KILLER_2,
        GENERATE_QUIET,
        QUIET,
        BAD_NOISY,
        DONE,
    };

    Stage       initial_stage() const;
    bool        includes_quiet_stages() const;
    bool        includes_killer_stages() const;
    bool        is_capture(const Move& move) const;
    bool        is_quiet(const Move& move) const;
    bool        is_pv_or_tt_duplicate(const Move& move) const;
    bool        is_quiet_hint_duplicate(const Move& move) const;
    bool        is_valid_hint(Move move) const;
    bool        is_valid_tt_hint(Move move) const;
    bool        is_returnable_killer(Move move, Move previous_killer) const;
    ScoredMove* moves_begin() { return moves.data(); }
    ScoredMove* moves_limit() { return moves.data() + MAX_MOVES; }
    void        score_noisy_moves(ScoredMove* begin, ScoredMove* end);
    void        score_quiet_moves(ScoredMove* begin, ScoredMove* end);
    void        score_evasion_moves(ScoredMove* begin, ScoredMove* end);
    Move consume(ScoredMove*& cursor, ScoredMove* picked, MoveScore score, MovePickSource source);
    void generate_noisy_moves();
    void generate_quiet_moves();
    void generate_evasion_moves();

    template <typename Predicate>
    Move
    pick_best(ScoredMove*& cursor, ScoredMove* end, Predicate predicate, MovePickSource source);

    MovePickerContext                 ctx;
    MovePickerMode                    mode;
    bool                              in_check;
    bool                              valid_pv_move;
    bool                              valid_tt_move;
    Stage                             stage;
    std::array<ScoredMove, MAX_MOVES> moves;
    ScoredMove*                       primary_cur{nullptr};
    ScoredMove*                       quiet_cur{nullptr};
    ScoredMove*                       end_primary{nullptr};
    ScoredMove*                       end_generated{nullptr};
    Move                              killer_1{NULL_MOVE};
    Move                              killer_2{NULL_MOVE};
    bool                              noisy_ready{false};
    bool                              quiet_ready{false};
    bool                              evasion_ready{false};
    bool                              skip_quiets{false};
    MoveScore                         last_picked_score{0};
    MovePickSource                    last_picked_source{MovePickSource::None};
};
