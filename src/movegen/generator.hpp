#pragma once

#include <cassert>

#include "board/board.hpp"
#include "core/attacks.hpp"
#include "core/move.hpp"
#include "core/move_geometry.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "core/types.hpp"
#include "movegen/move_list.hpp"

namespace movegen {

enum class MoveGenType { NonEvasions, Noisy, Evasions, Quiet };

// Color- and mode-specialized generator kept header-defined for hot-path inlining.
template <MoveGenType Type, Color Us>
class Generator {
public:
    Generator(const Board& board, MoveList& moves)
        : board(board),
          moves(moves),
          king_sq(board.king_sq(Us)),
          occupancy(board.occupancy()),
          own_pieces(board.pieces(Us)),
          enemy_pieces(board.pieces(~Us)) {}

    // Preserve emission order because it affects downstream move ordering.
    void run() {
        Bitboard target_squares = 0;

        if constexpr (Type == MoveGenType::Evasions) {
            // Only king moves can evade double check.
            if (!board.is_double_check()) {
                target_squares = mode_targets();
                piece_moves<KNIGHT>(target_squares);
                piece_moves<BISHOP>(target_squares);
                piece_moves<ROOK>(target_squares);
                piece_moves<QUEEN>(target_squares);
                pawn_moves(target_squares);
            }

            target_squares = ~own_pieces;
        } else {
            target_squares = mode_targets();
            piece_moves<KNIGHT>(target_squares);
            piece_moves<BISHOP>(target_squares);
            piece_moves<ROOK>(target_squares);
            piece_moves<QUEEN>(target_squares);
            pawn_moves(target_squares);
        }

        king_moves(target_squares);
    }

private:
    void emit(Square from, Square to, MoveType type = BASIC_MOVE, PieceType promotion = KNIGHT) {
        moves.add(from, to, type, promotion);
    }

    // Restricts non-king destinations according to the requested generation mode.
    Bitboard mode_targets() const {
        if constexpr (Type == MoveGenType::NonEvasions)
            return ~own_pieces;
        else if constexpr (Type == MoveGenType::Noisy)
            return enemy_pieces;
        else if constexpr (Type == MoveGenType::Evasions) {
            const Bitboard checks = board.checkers();
            return checks | square::between(bb::frontmost<Us>(checks), king_sq);
        }
        return ~occupancy;
    }

    template <PieceType P>
    void piece_moves(Bitboard targets) {
        Bitboard bitboard = board.pieces<P>(Us);

        bb::scan<Us>(bitboard, [&](Square from) {
            Bitboard moves = attacks::piece_moves<P>(from, occupancy) & targets;
            bb::scan<Us>(moves, [&](Square to) { emit(from, to); });
        });
    }

    // Pawn move bitboards encode destinations; recover each source by reversing the shift.
    template <int Delta>
    void emit_pawn_moves(Bitboard moves) {
        constexpr int offset = (Us == WHITE) ? -Delta : Delta;
        bb::scan<Us>(moves, [&](Square to) { emit(to + offset, to); });
    }

    template <int Delta>
    void emit_promotions(Bitboard moves) {
        constexpr int offset = (Us == WHITE) ? -Delta : Delta;

        bb::scan<Us>(moves, [&](Square to) {
            Square from = to + offset;
            // Emit promotions from highest to lowest material value.
            emit(from, to, MOVE_PROM, QUEEN);
            emit(from, to, MOVE_PROM, ROOK);
            emit(from, to, MOVE_PROM, BISHOP);
            emit(from, to, MOVE_PROM, KNIGHT);
        });
    }

    template <int Delta>
    void emit_enpassant(Bitboard pawns, Square target) {
        constexpr int offset = (Us == WHITE) ? -Delta : Delta;

        if (bb::contains(attacks::pawn_shift<Delta, Us>(pawns), target))
            emit(target + offset, target, MOVE_EP);
    }

    // Separates promotion pawns because every promotion is noisy.
    void pawn_moves(Bitboard targets) {
        constexpr Bitboard promotion_rank = bb::relative_rank<Us>(RANK7);

        Bitboard enemies = enemy_pieces;
        if constexpr (Type == MoveGenType::Evasions)
            enemies &= targets;

        const Bitboard all_pawns       = board.pieces<PAWN>(Us);
        Bitboard       promotion_pawns = all_pawns & promotion_rank;
        if constexpr (Type != MoveGenType::Quiet) {
            if (promotion_pawns)
                pawn_promotions(targets, enemies, promotion_pawns);
        }

        Bitboard normal_pawns = all_pawns & ~promotion_rank;
        if constexpr (Type != MoveGenType::Quiet)
            pawn_captures(targets, enemies, normal_pawns);
        if constexpr (Type != MoveGenType::Noisy)
            pawn_pushes(targets, normal_pawns);
    }

    void pawn_promotions(Bitboard targets, Bitboard enemies, Bitboard pawns) {
        Bitboard push_moves = attacks::pawn_shift<pawn_delta::push, Us>(pawns) & ~occupancy;
        if constexpr (Type == MoveGenType::Evasions)
            push_moves &= targets;

        emit_promotions<pawn_delta::push>(push_moves);

        Bitboard left_moves = attacks::pawn_shift<pawn_delta::left, Us>(pawns) & enemies;
        emit_promotions<pawn_delta::left>(left_moves);

        Bitboard right_moves = attacks::pawn_shift<pawn_delta::right, Us>(pawns) & enemies;
        emit_promotions<pawn_delta::right>(right_moves);
    }

    void pawn_captures(Bitboard targets, Bitboard enemies, Bitboard pawns) {
        Bitboard left_moves  = attacks::pawn_shift<pawn_delta::left, Us>(pawns) & enemies;
        Bitboard right_moves = attacks::pawn_shift<pawn_delta::right, Us>(pawns) & enemies;
        emit_pawn_moves<pawn_delta::left>(left_moves);
        emit_pawn_moves<pawn_delta::right>(right_moves);

        const Square enpassant_target = board.legal_enpassant_target();
        if (enpassant_target == INVALID)
            return;

        if constexpr (Type == MoveGenType::Evasions) {
            const Square captured = move_geometry::enpassant_captured_square(enpassant_target, Us);
            // En passant can evade by removing the checker or interposing on the destination.
            if (!bb::contains(targets, captured) && !bb::contains(targets, enpassant_target))
                return;
        }

        emit_enpassant<pawn_delta::left>(pawns, enpassant_target);
        emit_enpassant<pawn_delta::right>(pawns, enpassant_target);
    }

    void pawn_pushes(Bitboard targets, Bitboard pawns) {
        constexpr Bitboard double_push_rank = bb::relative_rank<Us>(RANK3);

        Bitboard push_moves = attacks::pawn_shift<pawn_delta::push, Us>(pawns) & ~occupancy;
        // Deriving double pushes from available single pushes also checks the intermediate square.
        Bitboard double_push_moves =
            attacks::pawn_shift<pawn_delta::push, Us>(push_moves & double_push_rank) & ~occupancy;

        if constexpr (Type == MoveGenType::Evasions) {
            push_moves &= targets;
            double_push_moves &= targets;
        }

        emit_pawn_moves<pawn_delta::double_push>(double_push_moves);
        emit_pawn_moves<pawn_delta::push>(push_moves);
    }

    void king_moves(Bitboard targets) {
        Bitboard moves = attacks::piece_moves<KING>(king_sq) & targets;

        bb::scan<Us>(moves, [&](Square to) { emit(king_sq, to); });

        // Castling is quiet and cannot be an evasion because the king is already in check.
        if constexpr (Type == MoveGenType::NonEvasions || Type == MoveGenType::Quiet) {
            if (board.has_castling_rights(Us)) {
                constexpr const auto& kingside  = move_geometry::castling(CASTLE_KINGSIDE, Us);
                constexpr const auto& queenside = move_geometry::castling(CASTLE_QUEENSIDE, Us);

                if (board.has_castling_right(CASTLE_KINGSIDE, Us) && can_castle<CASTLE_KINGSIDE>())
                    emit(kingside.king_from, kingside.king_to, MOVE_CASTLE);
                if (board.has_castling_right(CASTLE_QUEENSIDE, Us)
                    && can_castle<CASTLE_QUEENSIDE>())
                    emit(queenside.king_from, queenside.king_to, MOVE_CASTLE);
            }
        }
    }

    // Castling rights are checked by the caller; verify the empty and safe king paths.
    template <CastleSide Side>
    bool can_castle() const {
        constexpr const auto& castling = move_geometry::castling(Side, Us);
        return !(occupancy & castling.empty_path) && !board.any_attacked(castling.king_path, ~Us);
    }

    const Board& board;
    MoveList&    moves;
    Square       king_sq;
    Bitboard     occupancy;
    Bitboard     own_pieces;
    Bitboard     enemy_pieces;
};

template <MoveGenType Type>
inline MoveList generate(const Board& board) {
    // Dispatch runtime side-to-move into a color-specialized hot path.
    MoveList moves;
    if (board.side_to_move() == WHITE)
        Generator<Type, WHITE>(board, moves).run();
    else
        Generator<Type, BLACK>(board, moves).run();
    return moves;
}

// Generators emit pseudo-legal candidates; callers use Board for final king-safety filtering.
inline MoveList generate_non_evasions(const Board& board) {
    assert(!board.is_check());
    return generate<MoveGenType::NonEvasions>(board);
}

inline MoveList generate_noisy(const Board& board) {
    assert(!board.is_check());
    return generate<MoveGenType::Noisy>(board);
}

inline MoveList generate_quiet(const Board& board) {
    assert(!board.is_check());
    return generate<MoveGenType::Quiet>(board);
}

inline MoveList generate_evasions(const Board& board) {
    assert(board.is_check());
    return generate<MoveGenType::Evasions>(board);
}

inline MoveList generate_pseudo_legal(const Board& board) {
    return board.is_check() ? generate_evasions(board) : generate_non_evasions(board);
}

} // namespace movegen
