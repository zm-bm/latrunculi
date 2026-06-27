#pragma once

// Internal template move emitter. Public callers should include movegen.hpp instead.
#include <cstdint>

#include "board.hpp"
#include "defs.hpp"
#include "move.hpp"

namespace movegen {

enum class MoveGenType { NonEvasions, Noisy, Evasions, Quiet };

namespace impl {

template <typename MoveOutput>
struct GeneratorContext {
    const Board& board;
    MoveOutput&  output;
    Square       king_sq;
    uint64_t     occupancy;
};

template <PawnMove P, Color C, typename MoveOutput>
void add_pawn_moves(uint64_t bitboard, MoveOutput& output) {
    constexpr int offset = (C == WHITE) ? -P : P;

    bb::scan<C>(bitboard, [&](Square to) { output.add(to + offset, to); });
}

template <PawnMove P, Color C, typename MoveOutput>
void add_pawn_promotions(uint64_t bitboard, MoveOutput& output) {
    constexpr int offset = (C == WHITE) ? -P : P;

    bb::scan<C>(bitboard, [&](Square to) {
        Square from = to + offset;
        output.add(from, to, MOVE_PROM, QUEEN);
        output.add(from, to, MOVE_PROM, ROOK);
        output.add(from, to, MOVE_PROM, BISHOP);
        output.add(from, to, MOVE_PROM, KNIGHT);
    });
}

template <PawnMove P, Color C, typename MoveOutput>
void add_pawn_enpassants(uint64_t pawns, Square enpassant, MoveOutput& output) {
    constexpr int offset = (C == WHITE) ? -P : P;

    if (bb::pawn_moves<P, C>(pawns) & bb::set(enpassant))
        output.add(enpassant + offset, enpassant, MOVE_EP);
}

template <Color C, typename MoveOutput>
bool legal_kingside_castle(const GeneratorContext<MoveOutput>& ctx) {
    constexpr uint64_t path     = castle::path[CASTLE_KINGSIDE][C];
    constexpr uint64_t kingpath = castle::kingpath[CASTLE_KINGSIDE][C];
    return !(ctx.occupancy & path) && !ctx.board.attacks_to(kingpath, ~C);
}

template <Color C, typename MoveOutput>
bool legal_queenside_castle(const GeneratorContext<MoveOutput>& ctx) {
    constexpr uint64_t path     = castle::path[CASTLE_QUEENSIDE][C];
    constexpr uint64_t kingpath = castle::kingpath[CASTLE_QUEENSIDE][C];
    return !(ctx.occupancy & path) && !ctx.board.attacks_to(kingpath, ~C);
}

template <MoveGenType M, Color C, typename MoveOutput>
uint64_t piece_move_targets(GeneratorContext<MoveOutput>& ctx) {
    constexpr Color Opp = ~C;

    if constexpr (M == MoveGenType::NonEvasions)
        return ~ctx.board.template pieces<ALL_PIECES>(C);

    else if constexpr (M == MoveGenType::Noisy)
        return ctx.board.template pieces<ALL_PIECES>(Opp);

    else if constexpr (M == MoveGenType::Evasions) {
        const uint64_t checks = ctx.board.checkers();
        return checks | bb::between(bb::select<C>(checks), ctx.king_sq);
    }
    return ~ctx.occupancy;
}

template <PieceType P, Color C, typename MoveOutput>
void piece_moves(const GeneratorContext<MoveOutput>& ctx, uint64_t target_squares) {
    uint64_t bitboard = ctx.board.template pieces<P>(C);

    bb::scan<C>(bitboard, [&](Square from) {
        uint64_t moves = bb::moves<P>(from, ctx.occupancy) & target_squares;
        bb::scan<C>(moves, [&](Square to) { ctx.output.add(from, to); });
    });
}

template <MoveGenType M, Color C, typename MoveOutput>
void pawn_promotions(const GeneratorContext<MoveOutput>& ctx,
                     uint64_t                            target_squares,
                     uint64_t                            enemies,
                     uint64_t                            pawns) {
    uint64_t push_moves = bb::pawn_moves<PAWN_PUSH, C>(pawns) & ~ctx.occupancy;
    if constexpr (M == MoveGenType::Evasions)
        push_moves &= target_squares;

    add_pawn_promotions<PAWN_PUSH, C>(push_moves, ctx.output);

    uint64_t left_moves = bb::pawn_moves<PAWN_LEFT, C>(pawns) & enemies;
    add_pawn_promotions<PAWN_LEFT, C>(left_moves, ctx.output);

    uint64_t right_moves = bb::pawn_moves<PAWN_RIGHT, C>(pawns) & enemies;
    add_pawn_promotions<PAWN_RIGHT, C>(right_moves, ctx.output);
}

template <MoveGenType M, Color C, typename MoveOutput>
void pawn_captures(const GeneratorContext<MoveOutput>& ctx,
                   uint64_t                            target_squares,
                   uint64_t                            enemies,
                   uint64_t                            pawns) {
    uint64_t left_moves  = bb::pawn_moves<PAWN_LEFT, C>(pawns) & enemies;
    uint64_t right_moves = bb::pawn_moves<PAWN_RIGHT, C>(pawns) & enemies;
    add_pawn_moves<PAWN_LEFT, C>(left_moves, ctx.output);
    add_pawn_moves<PAWN_RIGHT, C>(right_moves, ctx.output);

    Square enpassant = ctx.board.legal_enpassant_sq();
    if (enpassant == INVALID)
        return;

    constexpr int offset = (C == WHITE) ? -PAWN_PUSH : PAWN_PUSH;
    if constexpr (M == MoveGenType::Evasions) {
        if (!(target_squares & bb::set(enpassant + offset)))
            return;
    }

    add_pawn_enpassants<PAWN_LEFT, C>(pawns, enpassant, ctx.output);
    add_pawn_enpassants<PAWN_RIGHT, C>(pawns, enpassant, ctx.output);
}

template <MoveGenType M, Color C, typename MoveOutput>
void pawn_pushes(const GeneratorContext<MoveOutput>& ctx, uint64_t target_squares, uint64_t pawns) {
    constexpr uint64_t rank3 = (C == WHITE) ? bb::rank(RANK3) : bb::rank(RANK6);

    uint64_t push_moves  = bb::pawn_moves<PAWN_PUSH, C>(pawns) & ~ctx.occupancy;
    uint64_t push2_moves = bb::pawn_moves<PAWN_PUSH, C>(push_moves & rank3) & ~ctx.occupancy;

    if constexpr (M == MoveGenType::Evasions) {
        push_moves  &= target_squares;
        push2_moves &= target_squares;
    }

    add_pawn_moves<PAWN_PUSH2, C>(push2_moves, ctx.output);
    add_pawn_moves<PAWN_PUSH, C>(push_moves, ctx.output);
}

template <MoveGenType M, Color C, typename MoveOutput>
void pawn_moves(const GeneratorContext<MoveOutput>& ctx, uint64_t target_squares) {
    constexpr Color    enemy = ~C;
    constexpr uint64_t rank7 = (C == WHITE) ? bb::rank(RANK7) : bb::rank(RANK2);

    uint64_t enemies = ctx.board.template pieces<ALL_PIECES>(enemy);
    if constexpr (M == MoveGenType::Evasions)
        enemies &= target_squares;

    uint64_t pawns_rank7 = ctx.board.template pieces<PAWN>(C) & rank7;
    if constexpr (M != MoveGenType::Quiet) {
        if (pawns_rank7)
            pawn_promotions<M, C>(ctx, target_squares, enemies, pawns_rank7);
    }

    uint64_t pawns = ctx.board.template pieces<PAWN>(C) & ~rank7;
    if constexpr (M != MoveGenType::Quiet)
        pawn_captures<M, C>(ctx, target_squares, enemies, pawns);
    if constexpr (M != MoveGenType::Noisy)
        pawn_pushes<M, C>(ctx, target_squares, pawns);
}

template <MoveGenType M, Color C, typename MoveOutput>
void king_moves(const GeneratorContext<MoveOutput>& ctx, uint64_t target_squares) {
    uint64_t moves = bb::moves<KING>(ctx.king_sq) & target_squares;

    bb::scan<C>(moves, [&](Square to) { ctx.output.add(ctx.king_sq, to); });

    if constexpr (M == MoveGenType::NonEvasions || M == MoveGenType::Quiet) {
        if (ctx.board.can_castle(C)) {
            constexpr Square from         = (C == WHITE) ? E1 : E8;
            constexpr Square to_kingside  = (C == WHITE) ? G1 : G8;
            constexpr Square to_queenside = (C == WHITE) ? C1 : C8;

            if (ctx.board.can_castle_kingside(C) && legal_kingside_castle<C>(ctx))
                ctx.output.add(from, to_kingside, MOVE_CASTLE);
            if (ctx.board.can_castle_queenside(C) && legal_queenside_castle<C>(ctx))
                ctx.output.add(from, to_queenside, MOVE_CASTLE);
        }
    }
}

template <MoveGenType M, Color C, typename MoveOutput>
void build_moves(const Board& board, MoveOutput& output) {
    GeneratorContext<MoveOutput> ctx{
        .board     = board,
        .output    = output,
        .king_sq   = board.king_sq(C),
        .occupancy = board.occupancy(),
    };

    uint64_t target_squares = 0;

    if constexpr (M == MoveGenType::Evasions) {
        if (!board.is_double_check()) {
            target_squares = piece_move_targets<M, C>(ctx);
            piece_moves<KNIGHT, C>(ctx, target_squares);
            piece_moves<BISHOP, C>(ctx, target_squares);
            piece_moves<ROOK, C>(ctx, target_squares);
            piece_moves<QUEEN, C>(ctx, target_squares);
            pawn_moves<M, C>(ctx, target_squares);
        }

        target_squares = ~ctx.board.template pieces<ALL_PIECES>(C);
    } else {
        target_squares = piece_move_targets<M, C>(ctx);
        piece_moves<KNIGHT, C>(ctx, target_squares);
        piece_moves<BISHOP, C>(ctx, target_squares);
        piece_moves<ROOK, C>(ctx, target_squares);
        piece_moves<QUEEN, C>(ctx, target_squares);
        pawn_moves<M, C>(ctx, target_squares);
    }

    king_moves<M, C>(ctx, target_squares);
}

} // namespace impl

template <MoveGenType M, typename MoveOutput>
inline void generate_into(const Board& board, MoveOutput& output) {
    const Color side = board.side_to_move();

    side == WHITE ? impl::build_moves<M, WHITE>(board, output)
                  : impl::build_moves<M, BLACK>(board, output);
}

} // namespace movegen
