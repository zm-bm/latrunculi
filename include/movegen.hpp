#pragma once

#include <cassert>
#include <cstdint>

#include "board.hpp"
#include "defs.hpp"
#include "move.hpp"
#include "move_list.hpp"

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
          own_pieces(board.template pieces<ALL_PIECES>(Us)),
          enemy_pieces(board.template pieces<ALL_PIECES>(~Us)),
          empty_squares(~occupancy) {}

    void run() {
        uint64_t target_squares = 0;

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

    uint64_t piece_targets() const {
        if constexpr (Type == MoveGenType::NonEvasions)
            return ~own_pieces;
        else if constexpr (Type == MoveGenType::Noisy)
            return enemy_pieces;
        else if constexpr (Type == MoveGenType::Evasions) {
            const uint64_t checks = board.checkers();
            return checks | bb::between(bb::select<Us>(checks), king_sq);
        }
        return empty_squares;
    }

    template <PieceType P>
    void pieces(uint64_t targets) {
        uint64_t bitboard = board.template pieces<P>(Us);

        bb::scan<Us>(bitboard, [&](Square from) {
            uint64_t moves = bb::moves<P>(from, occupancy) & targets;
            bb::scan<Us>(moves, [&](Square to) { write(from, to); });
        });
    }

    template <PawnMove P>
    void pawn_moves_to(uint64_t moves) {
        constexpr int offset = (Us == WHITE) ? -P : P;
        bb::scan<Us>(moves, [&](Square to) { write(to + offset, to); });
    }

    template <PawnMove P>
    void pawn_promotions_to(uint64_t moves) {
        constexpr int offset = (Us == WHITE) ? -P : P;

        bb::scan<Us>(moves, [&](Square to) {
            Square from = to + offset;
            write(from, to, MOVE_PROM, QUEEN);
            write(from, to, MOVE_PROM, ROOK);
            write(from, to, MOVE_PROM, BISHOP);
            write(from, to, MOVE_PROM, KNIGHT);
        });
    }

    template <PawnMove P>
    void pawn_enpassants(uint64_t pawns, Square enpassant) {
        constexpr int offset = (Us == WHITE) ? -P : P;

        if (bb::pawn_moves<P, Us>(pawns) & bb::set(enpassant))
            write(enpassant + offset, enpassant, MOVE_EP);
    }

    void pawns(uint64_t targets) {
        constexpr uint64_t rank7 = (Us == WHITE) ? bb::rank(RANK7) : bb::rank(RANK2);

        uint64_t enemies = enemy_pieces;
        if constexpr (Type == MoveGenType::Evasions)
            enemies &= targets;

        const uint64_t all_pawns   = board.template pieces<PAWN>(Us);
        uint64_t       pawns_rank7 = all_pawns & rank7;
        if constexpr (Type != MoveGenType::Quiet) {
            if (pawns_rank7)
                pawn_promotions(targets, enemies, pawns_rank7);
        }

        uint64_t normal_pawns = all_pawns & ~rank7;
        if constexpr (Type != MoveGenType::Quiet)
            pawn_captures(targets, enemies, normal_pawns);
        if constexpr (Type != MoveGenType::Noisy)
            pawn_pushes(targets, normal_pawns);
    }

    void pawn_promotions(uint64_t targets, uint64_t enemies, uint64_t pawns) {
        uint64_t push_moves = bb::pawn_moves<PAWN_PUSH, Us>(pawns) & ~occupancy;
        if constexpr (Type == MoveGenType::Evasions)
            push_moves &= targets;

        pawn_promotions_to<PAWN_PUSH>(push_moves);

        uint64_t left_moves = bb::pawn_moves<PAWN_LEFT, Us>(pawns) & enemies;
        pawn_promotions_to<PAWN_LEFT>(left_moves);

        uint64_t right_moves = bb::pawn_moves<PAWN_RIGHT, Us>(pawns) & enemies;
        pawn_promotions_to<PAWN_RIGHT>(right_moves);
    }

    void pawn_captures(uint64_t targets, uint64_t enemies, uint64_t pawns) {
        uint64_t left_moves  = bb::pawn_moves<PAWN_LEFT, Us>(pawns) & enemies;
        uint64_t right_moves = bb::pawn_moves<PAWN_RIGHT, Us>(pawns) & enemies;
        pawn_moves_to<PAWN_LEFT>(left_moves);
        pawn_moves_to<PAWN_RIGHT>(right_moves);

        Square enpassant = board.legal_enpassant_sq();
        if (enpassant == INVALID)
            return;

        constexpr int offset = (Us == WHITE) ? -PAWN_PUSH : PAWN_PUSH;
        if constexpr (Type == MoveGenType::Evasions) {
            if (!(targets & bb::set(enpassant + offset)))
                return;
        }

        pawn_enpassants<PAWN_LEFT>(pawns, enpassant);
        pawn_enpassants<PAWN_RIGHT>(pawns, enpassant);
    }

    void pawn_pushes(uint64_t targets, uint64_t pawns) {
        constexpr uint64_t rank3 = (Us == WHITE) ? bb::rank(RANK3) : bb::rank(RANK6);

        uint64_t push_moves  = bb::pawn_moves<PAWN_PUSH, Us>(pawns) & ~occupancy;
        uint64_t push2_moves = bb::pawn_moves<PAWN_PUSH, Us>(push_moves & rank3) & ~occupancy;

        if constexpr (Type == MoveGenType::Evasions) {
            push_moves  &= targets;
            push2_moves &= targets;
        }

        pawn_moves_to<PAWN_PUSH2>(push2_moves);
        pawn_moves_to<PAWN_PUSH>(push_moves);
    }

    void king(uint64_t targets) {
        uint64_t moves = bb::moves<KING>(king_sq) & targets;

        bb::scan<Us>(moves, [&](Square to) { write(king_sq, to); });

        if constexpr (Type == MoveGenType::NonEvasions || Type == MoveGenType::Quiet) {
            if (board.can_castle(Us)) {
                constexpr Square from         = (Us == WHITE) ? E1 : E8;
                constexpr Square to_kingside  = (Us == WHITE) ? G1 : G8;
                constexpr Square to_queenside = (Us == WHITE) ? C1 : C8;

                if (board.can_castle_kingside(Us) && legal_castle<CASTLE_KINGSIDE>())
                    write(from, to_kingside, MOVE_CASTLE);
                if (board.can_castle_queenside(Us) && legal_castle<CASTLE_QUEENSIDE>())
                    write(from, to_queenside, MOVE_CASTLE);
            }
        }
    }

    template <CastleSide Side>
    bool legal_castle() const {
        constexpr uint64_t path     = castle::path[Side][Us];
        constexpr uint64_t kingpath = castle::kingpath[Side][Us];
        return !(occupancy & path) && !board.attacks_to(kingpath, ~Us);
    }

    const Board& board;
    MoveList&    moves;
    Square       king_sq;
    uint64_t     occupancy;
    uint64_t     own_pieces;
    uint64_t     enemy_pieces;
    uint64_t     empty_squares;
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
