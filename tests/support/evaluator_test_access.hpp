#pragma once

#include "board/board.hpp"
#include "core/bitboard.hpp"
#include "eval/evaluator.hpp"

class EvaluatorTestAccess {
public:
    static Bitboard outposts(const eval::Evaluator& evaluator, Color color) {
        return evaluator.zones.outposts[color];
    }

    static Bitboard mobility_zone(const eval::Evaluator& evaluator, Color color) {
        return evaluator.zones.mobility[color];
    }

    template <Color C, PieceType P>
    static Bitboard
    piece_moves(const eval::Evaluator& evaluator, const Board& board, Square square) {
        constexpr Color                     Opp = ~C;
        const eval::Evaluator::PieceContext context{.square    = square,
                                                    .piece_bb  = bb::set(square),
                                                    .occupied  = board.occupancy(),
                                                    .pawns     = board.pieces<PAWN>(C),
                                                    .opp_pawns = board.pieces<PAWN>(Opp)};
        return evaluator.get_moves<C, P>(context);
    }

    template <Color C>
    static eval::TaperedScore shelter(const eval::Evaluator& evaluator, Square king_sq) {
        return evaluator.evaluate_shelter<C>(king_sq);
    }

    template <Color C>
    static eval::TaperedScore shelter_file(const eval::Evaluator& evaluator,
                                           Bitboard               pawns,
                                           Bitboard               opponent_pawns,
                                           File                   file) {
        return evaluator.evaluate_shelter_file<C>(pawns, opponent_pawns, file);
    }

    template <Color C>
    static int raw_danger(const eval::Evaluator& evaluator, Square king_sq) {
        return evaluator.calculate_raw_danger<C>(king_sq);
    }

    static int scale_factor(const eval::Evaluator& evaluator, Color color) {
        return evaluator.scale_factor(color);
    }

    static EvalValue taper_score(const eval::Evaluator& evaluator, eval::TaperedScore score) {
        return evaluator.taper_score(score);
    }

    static int phase(const eval::Evaluator& evaluator) { return evaluator.phase(); }
};
