#pragma once

#include <cassert>

#include "board/board.hpp"
#include "core/attacks.hpp"
#include "core/move.hpp"
#include "core/move_geometry.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "core/types.hpp"

namespace movegen {

enum class MoveGenType { NonEvasions, Noisy, Evasions, Quiet };

namespace detail {

// Board geometry for one synchronous pass; kind and output belong to the operation.
template <Color Us>
class Generator {
public:
    explicit Generator(const Board& board)
        : board(board),
          king_sq(board.king_sq(Us)),
          occupancy(board.occupancy()),
          own_pieces(board.pieces(Us)),
          enemy_pieces(board.pieces(~Us)) {}

    // Preserve emission order because it affects downstream move ordering.
    template <MoveGenType Type, typename Emit>
    void run(Emit& output) {
        Bitboard target_squares = 0;

        if constexpr (Type == MoveGenType::Evasions) {
            // Only king moves can evade double check.
            if (!board.is_double_check()) {
                target_squares = mode_targets<Type>();
                piece_moves<KNIGHT>(target_squares, output);
                piece_moves<BISHOP>(target_squares, output);
                piece_moves<ROOK>(target_squares, output);
                piece_moves<QUEEN>(target_squares, output);
                pawn_moves<Type>(target_squares, output);
            }

            target_squares = ~own_pieces;
        } else {
            target_squares = mode_targets<Type>();
            piece_moves<KNIGHT>(target_squares, output);
            piece_moves<BISHOP>(target_squares, output);
            piece_moves<ROOK>(target_squares, output);
            piece_moves<QUEEN>(target_squares, output);
            pawn_moves<Type>(target_squares, output);
        }

        king_moves<Type>(target_squares, output);
    }

private:
    template <typename Emit>
    static void emit(Emit&     output,
                     PieceType moving_piece,
                     Square    from,
                     Square    to,
                     MoveType  type      = BASIC_MOVE,
                     PieceType promotion = KNIGHT) {
        output(Move(from, to, type, promotion), moving_piece);
    }

    // Restricts non-king destinations according to the requested generation mode.
    template <MoveGenType Type>
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

    template <PieceType P, typename Emit>
    void piece_moves(Bitboard targets, Emit& output) {
        Bitboard bitboard = board.pieces<P>(Us);

        bb::scan<Us>(bitboard, [&](Square from) {
            Bitboard moves = attacks::piece_moves<P>(from, occupancy) & targets;
            bb::scan<Us>(moves, [&](Square to) { emit(output, P, from, to); });
        });
    }

    // Pawn move bitboards encode destinations; recover each source by reversing the shift.
    template <int Delta, typename Emit>
    void emit_pawn_moves(Bitboard moves, Emit& output) {
        constexpr int offset = (Us == WHITE) ? -Delta : Delta;
        bb::scan<Us>(moves, [&](Square to) { emit(output, PAWN, to + offset, to); });
    }

    template <int Delta, typename Emit>
    void emit_promotions(Bitboard moves, Emit& output) {
        constexpr int offset = (Us == WHITE) ? -Delta : Delta;

        bb::scan<Us>(moves, [&](Square to) {
            Square from = to + offset;
            // Emit promotions from highest to lowest material value.
            emit(output, PAWN, from, to, MOVE_PROM, QUEEN);
            emit(output, PAWN, from, to, MOVE_PROM, ROOK);
            emit(output, PAWN, from, to, MOVE_PROM, BISHOP);
            emit(output, PAWN, from, to, MOVE_PROM, KNIGHT);
        });
    }

    template <int Delta, typename Emit>
    void emit_enpassant(Bitboard pawns, Square target, Emit& output) {
        constexpr int offset = (Us == WHITE) ? -Delta : Delta;

        if (bb::contains(attacks::pawn_shift<Delta, Us>(pawns), target))
            emit(output, PAWN, target + offset, target, MOVE_EP);
    }

    // Separates promotion pawns because every promotion is noisy.
    template <MoveGenType Type, typename Emit>
    void pawn_moves(Bitboard targets, Emit& output) {
        constexpr Bitboard promotion_rank = bb::relative_rank<Us>(RANK7);

        Bitboard enemies = enemy_pieces;
        if constexpr (Type == MoveGenType::Evasions)
            enemies &= targets;

        const Bitboard all_pawns       = board.pieces<PAWN>(Us);
        Bitboard       promotion_pawns = all_pawns & promotion_rank;
        if constexpr (Type != MoveGenType::Quiet) {
            if (promotion_pawns)
                pawn_promotions<Type>(targets, enemies, promotion_pawns, output);
        }

        Bitboard normal_pawns = all_pawns & ~promotion_rank;
        if constexpr (Type != MoveGenType::Quiet)
            pawn_captures<Type>(targets, enemies, normal_pawns, output);
        if constexpr (Type != MoveGenType::Noisy)
            pawn_pushes<Type>(targets, normal_pawns, output);
    }

    template <MoveGenType Type, typename Emit>
    void pawn_promotions(Bitboard targets, Bitboard enemies, Bitboard pawns, Emit& output) {
        Bitboard push_moves = attacks::pawn_shift<pawn_delta::push, Us>(pawns) & ~occupancy;
        if constexpr (Type == MoveGenType::Evasions)
            push_moves &= targets;

        emit_promotions<pawn_delta::push>(push_moves, output);

        Bitboard left_moves = attacks::pawn_shift<pawn_delta::left, Us>(pawns) & enemies;
        emit_promotions<pawn_delta::left>(left_moves, output);

        Bitboard right_moves = attacks::pawn_shift<pawn_delta::right, Us>(pawns) & enemies;
        emit_promotions<pawn_delta::right>(right_moves, output);
    }

    template <MoveGenType Type, typename Emit>
    void pawn_captures(Bitboard targets, Bitboard enemies, Bitboard pawns, Emit& output) {
        Bitboard left_moves  = attacks::pawn_shift<pawn_delta::left, Us>(pawns) & enemies;
        Bitboard right_moves = attacks::pawn_shift<pawn_delta::right, Us>(pawns) & enemies;
        emit_pawn_moves<pawn_delta::left>(left_moves, output);
        emit_pawn_moves<pawn_delta::right>(right_moves, output);

        const Square enpassant_target = board.legal_enpassant_target();
        if (enpassant_target == INVALID)
            return;

        if constexpr (Type == MoveGenType::Evasions) {
            const Square captured = move_geometry::enpassant_captured_square(enpassant_target, Us);
            // En passant can evade by removing the checker or interposing on the destination.
            if (!bb::contains(targets, captured) && !bb::contains(targets, enpassant_target))
                return;
        }

        emit_enpassant<pawn_delta::left>(pawns, enpassant_target, output);
        emit_enpassant<pawn_delta::right>(pawns, enpassant_target, output);
    }

    template <MoveGenType Type, typename Emit>
    void pawn_pushes(Bitboard targets, Bitboard pawns, Emit& output) {
        constexpr Bitboard double_push_rank = bb::relative_rank<Us>(RANK3);

        Bitboard push_moves = attacks::pawn_shift<pawn_delta::push, Us>(pawns) & ~occupancy;
        // Deriving double pushes from available single pushes also checks the intermediate square.
        Bitboard double_push_moves =
            attacks::pawn_shift<pawn_delta::push, Us>(push_moves & double_push_rank) & ~occupancy;

        if constexpr (Type == MoveGenType::Evasions) {
            push_moves &= targets;
            double_push_moves &= targets;
        }

        emit_pawn_moves<pawn_delta::double_push>(double_push_moves, output);
        emit_pawn_moves<pawn_delta::push>(push_moves, output);
    }

    template <MoveGenType Type, typename Emit>
    void king_moves(Bitboard targets, Emit& output) {
        Bitboard moves = attacks::piece_moves<KING>(king_sq) & targets;

        bb::scan<Us>(moves, [&](Square to) { emit(output, KING, king_sq, to); });

        // Castling is quiet and cannot be an evasion because the king is already in check.
        if constexpr (Type == MoveGenType::NonEvasions || Type == MoveGenType::Quiet) {
            if (board.has_castling_rights(Us)) {
                constexpr const auto& kingside  = move_geometry::castling(CASTLE_KINGSIDE, Us);
                constexpr const auto& queenside = move_geometry::castling(CASTLE_QUEENSIDE, Us);

                if (board.has_castling_right(CASTLE_KINGSIDE, Us) && can_castle<CASTLE_KINGSIDE>())
                    emit(output, KING, kingside.king_from, kingside.king_to, MOVE_CASTLE);
                if (board.has_castling_right(CASTLE_QUEENSIDE, Us)
                    && can_castle<CASTLE_QUEENSIDE>())
                    emit(output, KING, queenside.king_from, queenside.king_to, MOVE_CASTLE);
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
    Square       king_sq;
    Bitboard     occupancy;
    Bitboard     own_pieces;
    Bitboard     enemy_pieces;
};

// Borrowed synchronous output receives (Move, original moving PieceType).
// Promotions and en passant report PAWN; castling reports KING.
template <MoveGenType Type, typename Emit>
inline void generate(const Board& board, Emit&& output) {
    assert((Type == MoveGenType::Evasions) == board.is_check());
    // Dispatch runtime side-to-move into a color-specialized hot path.
    if (board.side_to_move() == WHITE)
        Generator<WHITE>(board).run<Type>(output);
    else
        Generator<BLACK>(board).run<Type>(output);
}

// Ordinary loading shares the same emission protocol as ordered candidate batches.
template <typename Emit>
inline void generate_pseudo_legal(const Board& board, Emit&& output) {
    if (board.is_check())
        generate<MoveGenType::Evasions>(board, output);
    else
        generate<MoveGenType::NonEvasions>(board, output);
}

} // namespace detail

} // namespace movegen
