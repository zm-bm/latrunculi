#pragma once

#include "board/board.hpp"
#include "core/bitboard.hpp"
#include "eval/evaluator.hpp"

class EvaluatorTestAccess {
public:
    static Bitboard outposts(const Board& board, Color color) {
        const eval::Evaluator evaluator(board);
        return evaluator.zones.outposts[color];
    }

    static Bitboard mobility_zone(const Board& board, Color color) {
        const eval::Evaluator evaluator(board);
        return evaluator.zones.mobility[color];
    }

    template <Color C, PieceType P>
    static Bitboard piece_moves(const Board& board, Square square) {
        const eval::Evaluator               evaluator(board);
        constexpr Color                     Opp = ~C;
        const eval::Evaluator::PieceContext context{.square    = square,
                                                    .piece_bb  = bb::set(square),
                                                    .occupied  = board.occupancy(),
                                                    .pawns     = board.pieces<PAWN>(C),
                                                    .opp_pawns = board.pieces<PAWN>(Opp)};
        return evaluator.get_moves<C, P>(context);
    }

    template <Color C>
    static eval::TaperedScore shelter(const Board& board, Square king_sq) {
        const eval::Evaluator evaluator(board);
        return evaluator.evaluate_shelter<C>(king_sq);
    }

    template <Color C>
    static eval::TaperedScore
    shelter_file(const Board& board, Bitboard pawns, Bitboard opponent_pawns, File file) {
        const eval::Evaluator evaluator(board);
        return evaluator.evaluate_shelter_file<C>(pawns, opponent_pawns, file);
    }

    template <Color C>
    static int raw_danger(const Board& board, Square king_sq) {
        eval::Evaluator evaluator(board);
        (void)evaluator.evaluate();
        return evaluator.calculate_raw_danger<C>(king_sq);
    }

    static int scale_factor(const Board& board, Color color) {
        const eval::Evaluator evaluator(board);
        return evaluator.scale_factor(color);
    }

    static EvalValue taper_score(const Board& board, eval::TaperedScore score) {
        const eval::Evaluator evaluator(board);
        return evaluator.taper_score(score);
    }

    static int phase(const Board& board) {
        const eval::Evaluator evaluator(board);
        return evaluator.phase();
    }
};
