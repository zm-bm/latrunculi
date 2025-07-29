#pragma once

#include "board.hpp"
#include "defs.hpp"
#include "move_list.hpp"

struct GeneratorContext {
    const Board& board;
    MoveList&    movelist;
    Square       king_sq;
    uint64_t     occupancy;
};

namespace movegen {

template <GeneratorMode M, Color C>
void build_movelist(const Board& board, MoveList& movelist);

template <GeneratorMode M, Color C>
uint64_t piece_move_targets(GeneratorContext& ctx);

template <PieceType P, Color C>
void piece_moves(const GeneratorContext& ctx, const uint64_t targets);

template <GeneratorMode M, Color C>
void pawn_moves(const GeneratorContext& ctx, const int64_t targets);

template <GeneratorMode M, Color C>
void king_moves(const GeneratorContext& ctx, const uint64_t targets);

template <GeneratorMode M, Color C>
void pawn_promotions(const GeneratorContext& ctx,
                     const int64_t           targets,
                     const uint64_t          enemies,
                     const uint64_t          pawns);

template <GeneratorMode M, Color C>
void pawn_captures(const GeneratorContext& ctx,
                   const int64_t           targets,
                   const uint64_t          enemies,
                   const uint64_t          pawns);

template <GeneratorMode M, Color C>
void pawn_pushes(const GeneratorContext& ctx, const uint64_t targets, const uint64_t pawns);

template <PawnMove P, Color C>
void add_pawn_moves(uint64_t bitboard, MoveList& movelist);

template <PawnMove P, Color C>
void add_pawn_promotions(uint64_t bitboard, MoveList& movelist);

template <PawnMove P, Color C>
void add_pawn_enpassants(uint64_t pawns, Square enpassant, MoveList& movelist);

template <Color C>
bool legal_kingside_castle(const GeneratorContext& ctx);

template <Color C>
bool legal_queenside_castle(const GeneratorContext& ctx);

}; // namespace movegen

template <GeneratorMode M = ALL_MOVES>
inline MoveList generate(const Board& board) {
    MoveList movelist;
    Color    side = board.side_to_move();

    if (board.is_check()) {
        side == WHITE ? movegen::build_movelist<EVASIONS, WHITE>(board, movelist)
                      : movegen::build_movelist<EVASIONS, BLACK>(board, movelist);
    } else {
        side == WHITE ? movegen::build_movelist<M, WHITE>(board, movelist)
                      : movegen::build_movelist<M, BLACK>(board, movelist);
    }

    return movelist;
}
