#pragma once

#include "core/piece.hpp"
#include "core/square.hpp"
#include "eval/evaluation.hpp"
#include "eval/tapered_score.hpp"

class Board;
class EvaluatorTestAccess;

namespace eval {

// Internal single-use implementation of the public evaluation entry points.
class Evaluator {
private:
    Evaluator() = delete;
    explicit Evaluator(const Board&);
    Evaluator(const Evaluator&)            = delete;
    Evaluator& operator=(const Evaluator&) = delete;
    Evaluator(Evaluator&&)                 = delete;
    Evaluator& operator=(Evaluator&&)      = delete;

    [[nodiscard]] EvalValue evaluate();
    [[nodiscard]] Trace     trace();

    const Board& board;

    struct AttackData {
        Bitboard by[N_COLORS][N_PIECETYPES] = {{0}};
        Bitboard by2[N_COLORS]              = {0};
    } attacks;

    struct ZoneData {
        Bitboard outposts[N_COLORS] = {0};
        Bitboard mobility[N_COLORS] = {0};
        Bitboard king[N_COLORS]     = {0};
    } zones;

    struct KingAttackersData {
        int       count[N_COLORS] = {0};
        EvalValue value[N_COLORS] = {0};
    } king_attackers;

    struct ScoreData {
        TaperedScore mobility[N_COLORS] = {TaperedScore::Zero};
        TaperedScore threats[N_COLORS]  = {TaperedScore::Zero};
    } scores;

    struct PieceContext {
        Square   square    = INVALID;
        Bitboard piece_bb  = 0;
        Bitboard occupied  = 0;
        Bitboard pawns     = 0;
        Bitboard opp_pawns = 0;
    };

    template <Color C>
    void initialize();

    template <Color C>
    Bitboard outposts_zone(Bitboard pawns, Bitboard opp_pawns) const;

    template <Color C>
    Bitboard mobility_zone(Bitboard pawns, Bitboard opp_pawns, Square king_sq) const;

    template <Color C>
    Bitboard king_zone(Square king_sq) const;

    template <bool Tracing>
    EvalValue evaluate_impl(Trace* trace);

    template <bool Tracing, Term term, Color C = WHITE>
    TaperedScore evaluate_term(Trace* trace);

    template <Color C>
    TaperedScore evaluate_pawns();

    template <Color C, PieceType P>
    TaperedScore evaluate_pieces();

    template <Color C>
    TaperedScore evaluate_king_safety() const;

    template <Color C, PieceType P>
    void update_attacks(Bitboard moves);

    template <Color C, PieceType P>
    void update_mobility(Bitboard moves);

    template <Color C, PieceType P>
    void update_threats(const PieceContext& context);

    template <Color C, PieceType P>
    TaperedScore update_attackers(const PieceContext& context, Bitboard moves);

    template <Color C, PieceType P>
    Bitboard get_moves(const PieceContext& context) const;

    template <Color C, PieceType P>
    TaperedScore evaluate_minor_pieces(const PieceContext& context, Bitboard moves) const;

    template <Color C>
    TaperedScore evaluate_bishops(const PieceContext& context) const;

    template <Color C>
    TaperedScore evaluate_bishop_blockers(const PieceContext& context) const;

    template <Color C>
    TaperedScore evaluate_rook(const PieceContext& context) const;

    template <Color C>
    TaperedScore evaluate_queen(const PieceContext& context) const;

    template <Color C, PieceType P>
    bool discovery_attack(const PieceContext& context) const;

    template <Color C>
    TaperedScore evaluate_shelter(Square king_sq) const;

    template <Color C>
    TaperedScore evaluate_shelter_file(Bitboard pawns, Bitboard opp_pawns, File file) const;

    template <Color C>
    TaperedScore evaluate_danger(Square king_sq) const;

    template <Color C>
    int calculate_raw_danger(Square king_sq) const;

    template <PieceType P>
    int calculate_check_danger(Bitboard safe_checks, Bitboard all_checks) const;

    int       scale_factor(Color color) const;
    EvalValue taper_score(TaperedScore score) const;
    int       phase() const;

    friend EvalValue evaluate(const Board& board);
    friend Trace     evaluate_trace(const Board& board);
    friend class ::EvaluatorTestAccess;
};

} // namespace eval

#include "eval/evaluator_detail.hpp"
