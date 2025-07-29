#include "movegen.hpp"

namespace movegen {

template <GeneratorMode M, Color C>
void build_movelist(const Board& board, MoveList& movelist) {
    constexpr Color Opp = ~C;

    GeneratorContext ctx{
        .board     = board,
        .movelist  = movelist,
        .king_sq   = board.king_sq(C),
        .occupancy = board.occupancy(),
    };

    uint64_t targets = 0;

    if (M != EVASIONS || !board.is_double_check()) {
        targets = piece_move_targets<M, C>(ctx);
        movegen::piece_moves<KNIGHT, C>(ctx, targets);
        movegen::piece_moves<BISHOP, C>(ctx, targets);
        movegen::piece_moves<ROOK, C>(ctx, targets);
        movegen::piece_moves<QUEEN, C>(ctx, targets);
        movegen::pawn_moves<M, C>(ctx, targets);
    }

    if constexpr (M == EVASIONS)
        targets = ~ctx.board.pieces<ALL_PIECES>(C);
    movegen::king_moves<M, C>(ctx, targets);
}

template void build_movelist<ALL_MOVES, WHITE>(const Board& board, MoveList& movelist);
template void build_movelist<ALL_MOVES, BLACK>(const Board& board, MoveList& movelist);
template void build_movelist<CAPTURES, WHITE>(const Board& board, MoveList& movelist);
template void build_movelist<CAPTURES, BLACK>(const Board& board, MoveList& movelist);
template void build_movelist<EVASIONS, WHITE>(const Board& board, MoveList& movelist);
template void build_movelist<EVASIONS, BLACK>(const Board& board, MoveList& movelist);

template <GeneratorMode M, Color C>
uint64_t piece_move_targets(GeneratorContext& ctx) {
    constexpr Color Opp = ~C;

    if constexpr (M == ALL_MOVES)
        return ~ctx.board.pieces<ALL_PIECES>(C);

    else if constexpr (M == CAPTURES)
        return ctx.board.pieces<ALL_PIECES>(Opp);

    else if constexpr (M == EVASIONS) {
        const uint64_t checks = ctx.board.checkers();
        return checks | bb::between(bb::select<C>(checks), ctx.king_sq);
    }
    return ~ctx.occupancy;
}

template <PieceType P, Color C>
void piece_moves(const GeneratorContext& ctx, const uint64_t targets) {
    uint64_t bitboard = ctx.board.pieces<P>(C);

    bb::scan<C>(bitboard, [&](Square from) {
        uint64_t moves = bb::moves<P>(from, ctx.occupancy) & targets;
        bb::scan<C>(moves, [&](Square to) { ctx.movelist.add(from, to); });
    });
}

template <GeneratorMode M, Color C>
void pawn_moves(const GeneratorContext& ctx, const int64_t targets) {
    constexpr Color    enemy = ~C;
    constexpr uint64_t rank7 = (C == WHITE) ? bb::rank(RANK7) : bb::rank(RANK2);

    uint64_t enemies = ctx.board.pieces<ALL_PIECES>(enemy);
    if constexpr (M == EVASIONS)
        enemies &= targets; // only consider enemies that check our king

    uint64_t pawns_rank7 = ctx.board.pieces<PAWN>(C) & rank7;
    if (pawns_rank7)
        pawn_promotions<M, C>(ctx, targets, enemies, pawns_rank7);

    uint64_t pawns = ctx.board.pieces<PAWN>(C) & ~rank7;
    if constexpr (M != QUIETS)
        pawn_captures<M, C>(ctx, targets, enemies, pawns);
    if constexpr (M != CAPTURES)
        pawn_pushes<M, C>(ctx, targets, pawns);
}

template <GeneratorMode M, Color C>
void king_moves(const GeneratorContext& ctx, const uint64_t targets) {
    uint64_t moves = bb::moves<KING>(ctx.king_sq) & targets;

    bb::scan<C>(moves, [&](Square to) { ctx.movelist.add(ctx.king_sq, to); });

    if ((M == ALL_MOVES || M == QUIETS) && ctx.board.can_castle(C)) {
        constexpr Square from         = (C == WHITE) ? E1 : E8;
        constexpr Square to_kingside  = (C == WHITE) ? G1 : G8;
        constexpr Square to_queenside = (C == WHITE) ? C1 : C8;

        if (ctx.board.can_castle_kingside(C) && legal_kingside_castle<C>(ctx))
            ctx.movelist.add(from, to_kingside, MOVE_CASTLE);
        if (ctx.board.can_castle_queenside(C) && legal_queenside_castle<C>(ctx))
            ctx.movelist.add(from, to_queenside, MOVE_CASTLE);
    }
}

template <GeneratorMode M, Color C>
void pawn_promotions(const GeneratorContext& ctx,
                     const int64_t           targets,
                     const uint64_t          enemies,
                     const uint64_t          pawns) {

    uint64_t push_moves = bb::pawn_moves<PAWN_PUSH, C>(pawns) & ~ctx.occupancy;
    if constexpr (M == EVASIONS)
        push_moves &= targets;
    add_pawn_promotions<PAWN_PUSH, C>(push_moves, ctx.movelist);

    uint64_t left_moves = bb::pawn_moves<PAWN_LEFT, C>(pawns) & enemies;
    add_pawn_promotions<PAWN_LEFT, C>(left_moves, ctx.movelist);

    uint64_t right_moves = bb::pawn_moves<PAWN_RIGHT, C>(pawns) & enemies;
    add_pawn_promotions<PAWN_RIGHT, C>(right_moves, ctx.movelist);
}

template <GeneratorMode M, Color C>
void pawn_captures(const GeneratorContext& ctx,
                   const int64_t           targets,
                   const uint64_t          enemies,
                   const uint64_t          pawns) {

    uint64_t left_moves  = bb::pawn_moves<PAWN_LEFT, C>(pawns) & enemies;
    uint64_t right_moves = bb::pawn_moves<PAWN_RIGHT, C>(pawns) & enemies;
    add_pawn_moves<PAWN_LEFT, C>(left_moves, ctx.movelist);
    add_pawn_moves<PAWN_RIGHT, C>(right_moves, ctx.movelist);

    Square enpassant = ctx.board.enpassant_sq();
    if (enpassant == INVALID)
        return;

    constexpr int offset = (C == WHITE) ? -PAWN_PUSH : PAWN_PUSH;
    if (M != EVASIONS || (targets & bb::set(enpassant + offset))) {
        add_pawn_enpassants<PAWN_LEFT, C>(pawns, enpassant, ctx.movelist);
        add_pawn_enpassants<PAWN_RIGHT, C>(pawns, enpassant, ctx.movelist);
    }
}

template <GeneratorMode M, Color C>
void pawn_pushes(const GeneratorContext& ctx, const uint64_t targets, const uint64_t pawns) {
    constexpr uint64_t rank3 = (C == WHITE) ? bb::rank(RANK3) : bb::rank(RANK6);

    uint64_t push_moves  = bb::pawn_moves<PAWN_PUSH, C>(pawns) & ~ctx.occupancy;
    uint64_t push2_moves = bb::pawn_moves<PAWN_PUSH, C>(push_moves & rank3) & ~ctx.occupancy;

    if constexpr (M == EVASIONS) {
        push_moves  &= targets;
        push2_moves &= targets;
    }

    add_pawn_moves<PAWN_PUSH2, C>(push2_moves, ctx.movelist);
    add_pawn_moves<PAWN_PUSH, C>(push_moves, ctx.movelist);
}

template <PawnMove P, Color C>
void add_pawn_moves(uint64_t bitboard, MoveList& movelist) {
    constexpr int offset = (C == WHITE) ? -P : P;

    bb::scan<C>(bitboard, [&](Square to) { movelist.add(to + offset, to); });
};

template <PawnMove P, Color C>
void add_pawn_promotions(uint64_t bitboard, MoveList& movelist) {
    constexpr int offset = (C == WHITE) ? -P : P;

    bb::scan<C>(bitboard, [&](Square to) {
        Square from = to + offset;
        movelist.add(from, to, MOVE_PROM, QUEEN);
        movelist.add(from, to, MOVE_PROM, ROOK);
        movelist.add(from, to, MOVE_PROM, BISHOP);
        movelist.add(from, to, MOVE_PROM, KNIGHT);
    });
}

template <PawnMove P, Color C>
void add_pawn_enpassants(uint64_t pawns, Square enpassant, MoveList& movelist) {
    constexpr int offset = (C == WHITE) ? -P : P;

    if (bb::pawn_moves<P, C>(pawns) & bb::set(enpassant))
        movelist.add(enpassant + offset, enpassant, MOVE_EP);
}

template <Color C>
bool legal_kingside_castle(const GeneratorContext& ctx) {
    constexpr uint64_t path     = castle::path[CASTLE_KINGSIDE][C];
    constexpr uint64_t kingpath = castle::kingpath[CASTLE_KINGSIDE][C];
    return !(ctx.occupancy & path) && !ctx.board.attacks_to(kingpath, ~C);
}

template <Color C>
bool legal_queenside_castle(const GeneratorContext& ctx) {
    constexpr uint64_t path     = castle::path[CASTLE_QUEENSIDE][C];
    constexpr uint64_t kingpath = castle::kingpath[CASTLE_QUEENSIDE][C];
    return !(ctx.occupancy & path) && !ctx.board.attacks_to(kingpath, ~C);
}

}; // namespace movegen
