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

namespace impl {

template <MoveGenType Type, Color Us>
class Generator {
public:
    Generator(const Board& board, MoveList& moves)
        : board(board),
          moves(moves),
          king_sq(board.king_sq(Us)),
          occupancy(board.occupancy()),
          own_pieces(board.pieces(Us)),
          enemy_pieces(board.pieces(~Us)),
          empty_squares(~occupancy) {}

    void run() {
        Bitboard target_squares = 0;

        if constexpr (Type == MoveGenType::Evasions) {
            if (!board.is_double_check()) {
                target_squares = piece_targets();
                pieces<KNIGHT>(target_squares);
                pieces<BISHOP>(target_squares);
                pieces<ROOK>(target_squares);
                pieces<QUEEN>(target_squares);
                pawns(target_squares);
            }

            target_squares = ~own_pieces;
        } else {
            target_squares = piece_targets();
            pieces<KNIGHT>(target_squares);
            pieces<BISHOP>(target_squares);
            pieces<ROOK>(target_squares);
            pieces<QUEEN>(target_squares);
            pawns(target_squares);
        }

        king(target_squares);
    }

private:
    void write(Move move) { moves.add(move); }

    void
    write(Square from, Square to, MoveType move_type = BASIC_MOVE, PieceType promotion = KNIGHT) {
        write(Move(from, to, move_type, promotion));
    }

    Bitboard piece_targets() const {
        if constexpr (Type == MoveGenType::NonEvasions)
            return ~own_pieces;
        else if constexpr (Type == MoveGenType::Noisy)
            return enemy_pieces;
        else if constexpr (Type == MoveGenType::Evasions) {
            const Bitboard checks = board.checkers();
            return checks | square::between(bb::frontmost<Us>(checks), king_sq);
        }
        return empty_squares;
    }

    template <PieceType P>
    void pieces(Bitboard targets) {
        Bitboard bitboard = board.template pieces<P>(Us);

        bb::scan<Us>(bitboard, [&](Square from) {
            Bitboard moves = attacks::piece_moves<P>(from, occupancy) & targets;
            bb::scan<Us>(moves, [&](Square to) { write(from, to); });
        });
    }

    template <int Delta>
    void pawn_moves_to(Bitboard moves) {
        constexpr int offset = (Us == WHITE) ? -Delta : Delta;
        bb::scan<Us>(moves, [&](Square to) { write(to + offset, to); });
    }

    template <int Delta>
    void pawn_promotions_to(Bitboard moves) {
        constexpr int offset = (Us == WHITE) ? -Delta : Delta;

        bb::scan<Us>(moves, [&](Square to) {
            Square from = to + offset;
            write(from, to, MOVE_PROM, QUEEN);
            write(from, to, MOVE_PROM, ROOK);
            write(from, to, MOVE_PROM, BISHOP);
            write(from, to, MOVE_PROM, KNIGHT);
        });
    }

    template <int Delta>
    void pawn_enpassants(Bitboard pawns, Square target) {
        constexpr int offset = (Us == WHITE) ? -Delta : Delta;

        if (bb::contains(attacks::pawn_shift<Delta, Us>(pawns), target))
            write(target + offset, target, MOVE_EP);
    }

    void pawns(Bitboard targets) {
        constexpr Bitboard promotion_rank = bb::relative_rank<Us>(RANK7);

        Bitboard enemies = enemy_pieces;
        if constexpr (Type == MoveGenType::Evasions)
            enemies &= targets;

        const Bitboard all_pawns       = board.template pieces<PAWN>(Us);
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

        pawn_promotions_to<pawn_delta::push>(push_moves);

        Bitboard left_moves = attacks::pawn_shift<pawn_delta::left, Us>(pawns) & enemies;
        pawn_promotions_to<pawn_delta::left>(left_moves);

        Bitboard right_moves = attacks::pawn_shift<pawn_delta::right, Us>(pawns) & enemies;
        pawn_promotions_to<pawn_delta::right>(right_moves);
    }

    void pawn_captures(Bitboard targets, Bitboard enemies, Bitboard pawns) {
        Bitboard left_moves  = attacks::pawn_shift<pawn_delta::left, Us>(pawns) & enemies;
        Bitboard right_moves = attacks::pawn_shift<pawn_delta::right, Us>(pawns) & enemies;
        pawn_moves_to<pawn_delta::left>(left_moves);
        pawn_moves_to<pawn_delta::right>(right_moves);

        const Square enpassant_target = board.legal_enpassant_target();
        if (enpassant_target == INVALID)
            return;

        if constexpr (Type == MoveGenType::Evasions) {
            const Square captured = move_geometry::enpassant_captured_square(enpassant_target, Us);
            if (!bb::contains(targets, captured))
                return;
        }

        pawn_enpassants<pawn_delta::left>(pawns, enpassant_target);
        pawn_enpassants<pawn_delta::right>(pawns, enpassant_target);
    }

    void pawn_pushes(Bitboard targets, Bitboard pawns) {
        constexpr Bitboard double_push_rank = bb::relative_rank<Us>(RANK3);

        Bitboard push_moves = attacks::pawn_shift<pawn_delta::push, Us>(pawns) & ~occupancy;
        Bitboard double_push_moves =
            attacks::pawn_shift<pawn_delta::push, Us>(push_moves & double_push_rank) & ~occupancy;

        if constexpr (Type == MoveGenType::Evasions) {
            push_moves        &= targets;
            double_push_moves &= targets;
        }

        pawn_moves_to<pawn_delta::double_push>(double_push_moves);
        pawn_moves_to<pawn_delta::push>(push_moves);
    }

    void king(Bitboard targets) {
        Bitboard moves = attacks::piece_moves<KING>(king_sq) & targets;

        bb::scan<Us>(moves, [&](Square to) { write(king_sq, to); });

        if constexpr (Type == MoveGenType::NonEvasions || Type == MoveGenType::Quiet) {
            if (board.can_castle(Us)) {
                constexpr const auto& kingside  = move_geometry::castling(CASTLE_KINGSIDE, Us);
                constexpr const auto& queenside = move_geometry::castling(CASTLE_QUEENSIDE, Us);

                if (board.can_castle_kingside(Us) && legal_castle<CASTLE_KINGSIDE>())
                    write(kingside.king_from, kingside.king_to, MOVE_CASTLE);
                if (board.can_castle_queenside(Us) && legal_castle<CASTLE_QUEENSIDE>())
                    write(queenside.king_from, queenside.king_to, MOVE_CASTLE);
            }
        }
    }

    template <CastleSide Side>
    bool legal_castle() const {
        constexpr const auto& castling = move_geometry::castling(Side, Us);
        return !(occupancy & castling.empty_path) && !board.attacks_to(castling.king_path, ~Us);
    }

    const Board& board;
    MoveList&    moves;
    Square       king_sq;
    Bitboard     occupancy;
    Bitboard     own_pieces;
    Bitboard     enemy_pieces;
    Bitboard     empty_squares;
};

template <MoveGenType Type, Color Us>
inline void generate_for_color(const Board& board, MoveList& moves) {
    Generator<Type, Us>(board, moves).run();
}

template <MoveGenType Type>
inline void generate_into(const Board& board, MoveList& moves) {
    const Color side = board.side_to_move();
    if (side == WHITE)
        generate_for_color<Type, WHITE>(board, moves);
    else
        generate_for_color<Type, BLACK>(board, moves);
}

template <MoveGenType Type>
inline MoveList generate_movelist(const Board& board) {
    MoveList moves;
    generate_into<Type>(board, moves);
    return moves;
}

} // namespace impl

inline MoveList generate_non_evasions(const Board& board) {
    assert(!board.is_check());
    return impl::generate_movelist<MoveGenType::NonEvasions>(board);
}

inline MoveList generate_noisy(const Board& board) {
    assert(!board.is_check());
    return impl::generate_movelist<MoveGenType::Noisy>(board);
}

inline MoveList generate_quiet(const Board& board) {
    assert(!board.is_check());
    return impl::generate_movelist<MoveGenType::Quiet>(board);
}

inline MoveList generate_evasions(const Board& board) {
    assert(board.is_check());
    return impl::generate_movelist<MoveGenType::Evasions>(board);
}

inline MoveList generate_pseudo_legal(const Board& board) {
    return board.is_check() ? generate_evasions(board) : generate_non_evasions(board);
}

} // namespace movegen
