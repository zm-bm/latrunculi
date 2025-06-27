#pragma once

#include <array>
#include <iostream>
#include <vector>

#include "bb_utils.hpp"
#include "magic.hpp"
#include "types.hpp"

namespace BB {

using namespace BBUtils;

constexpr U64 rankBB(const Rank rank) { return 0xFFULL << (idx(rank) << 3); }
constexpr U64 fileBB(const File file) { return 0x0101010101010101ULL << idx(file); }

constexpr U64 set(const Square sq) { return 0x1ULL << idx(sq); }
constexpr U64 clear(const Square sq) { return ~set(sq); }

template <typename... Squares>
constexpr U64 set(Squares... sqs) {
    return (set(sqs) | ...);
}

constexpr BBMatrix DISTANCE = createBBMatrix(calcDistance);
constexpr U64 distance(const Square sq1, const Square sq2) { return DISTANCE[sq1][sq2]; }

constexpr BBMatrix COLLINEAR_MASK = createBBMatrix(calcCollinearMask);
constexpr U64 collinear(const Square sq1, const Square sq2) { return COLLINEAR_MASK[sq1][sq2]; }

constexpr BBMatrix BETWEEN_MASK = createBBMatrix(calcBetweenMask);
constexpr U64 between(const Square sq1, const Square sq2) { return BETWEEN_MASK[sq1][sq2]; }

constexpr BBTable KNIGHT_ATTACKS = createBBTable(makeCalcAttacks({
    // clang-format off
    {+2, -1}, {+1, -2}, {+1, +2}, {+2, +1},
    {-2, -1}, {-1, -2}, {-1, +2}, {-2, +1},
    // clang-format on
}));

constexpr BBTable KING_ATTACKS = createBBTable(makeCalcAttacks({
    // clang-format off
    {+1, -1}, {+1, 0}, {+1, +1},
    { 0, -1},          { 0, +1},
    {-1, -1}, {-1, 0}, {-1, +1},
    // clang-format on
}));

constexpr int count(const U64 bb) { return __builtin_popcountll(bb); }
constexpr U64 isMany(const U64 bb) { return bb & (bb - 1); }
constexpr Square lsb(const U64 bb) { return static_cast<Square>(__builtin_ctzll(bb)); }
constexpr Square msb(const U64 bb) { return static_cast<Square>(63 - __builtin_clzll(bb)); }

constexpr Square lsbPop(U64& bb) {
    auto sq  = lsb(bb);
    bb      &= clear(sq);
    return sq;
}
constexpr Square msbPop(U64& bb) {
    auto sq  = msb(bb);
    bb      &= clear(sq);
    return sq;
}

template <Color c>
constexpr Square advancedSq(U64 bb) {
    return (c == WHITE) ? msb(bb) : lsb(bb);
}

constexpr U64 fillNorth(U64 bb) {
    bb |= (bb << 8);
    bb |= (bb << 16);
    bb |= (bb << 32);
    return bb;
}

constexpr U64 fillSouth(U64 bb) {
    bb |= (bb >> 8);
    bb |= (bb >> 16);
    bb |= (bb >> 32);
    return bb;
}

constexpr U64 fillFiles(U64 bb) { return fillNorth(bb) | fillSouth(bb); }
constexpr U64 shiftSouth(U64 bb) { return bb >> 8; }
constexpr U64 shiftNorth(U64 bb) { return bb << 8; }
constexpr U64 shiftEast(U64 bb) { return (bb << 1) & ~BB::fileBB(File::F1); }
constexpr U64 shiftWest(U64 bb) { return (bb >> 1) & ~BB::fileBB(File::F8); }
constexpr U64 spanNorth(U64 bb) { return shiftNorth(fillNorth(bb)); }
constexpr U64 spanSouth(U64 bb) { return shiftSouth(fillSouth(bb)); }

template <Color c>
constexpr U64 spanFront(U64 bb) {
    return (c == WHITE) ? spanNorth(bb) : spanSouth(bb);
}

template <Color c>
constexpr U64 spanBack(U64 bb) {
    return (c == WHITE) ? spanSouth(bb) : spanNorth(bb);
}

template <Color c>
constexpr U64 pawnAttackSpan(U64 bb) {
    U64 spanFrontBB = spanFront<c>(bb);
    return shiftWest(spanFrontBB) | shiftEast(spanFrontBB);
}

template <Color c>
constexpr U64 pawnFullSpan(U64 bb) {
    U64 spanFrontBB = spanFront<c>(bb);
    return shiftWest(spanFrontBB) | shiftEast(spanFrontBB) | spanFrontBB;
}

template <PawnMove p, Color c>
constexpr U64 pawnMoves(U64 pawns) {
    if constexpr (p == PawnMove::Left || p == PawnMove::Right) {
        constexpr File edge  = ((p == PawnMove::Left) ^ (c == BLACK)) ? File::F1 : File::F8;
        pawns               &= ~fileBB(edge);
    }

    constexpr auto offset = idx(p);
    return (c == WHITE) ? pawns << offset : pawns >> offset;
}

template <PawnMove p>
constexpr U64 pawnMoves(U64 pawns, Color c) {
    return (c == WHITE) ? pawnMoves<p, WHITE>(pawns) : pawnMoves<p, BLACK>(pawns);
};

template <Color c>
constexpr U64 pawnAttacks(U64 pawns) {
    return pawnMoves<PawnMove::Left, c>(pawns) | pawnMoves<PawnMove::Right, c>(pawns);
}

constexpr U64 pawnAttacks(U64 pawns, Color c) {
    return pawnMoves<PawnMove::Left>(pawns, c) | pawnMoves<PawnMove::Right>(pawns, c);
}

template <Color c>
constexpr U64 pawnDoubleAttacks(U64 pawns) {
    return pawnMoves<PawnMove::Left, c>(pawns) & pawnMoves<PawnMove::Right, c>(pawns);
}

constexpr U64 pawnDoubleAttacks(U64 pawns, Color c) {
    return pawnMoves<PawnMove::Left>(pawns, c) & pawnMoves<PawnMove::Right>(pawns, c);
}

template <PieceType p>
constexpr U64 moves(Square sq, U64 occupancy = 0) {
    switch (p) {
        case PieceType::Knight: return KNIGHT_ATTACKS[sq];
        case PieceType::Bishop: return Magic::getBishopAttacks(sq, occupancy);
        case PieceType::Rook: return Magic::getRookAttacks(sq, occupancy);
        case PieceType::Queen: return Magic::getQueenAttacks(sq, occupancy);
        case PieceType::King: return KING_ATTACKS[sq];
        default: return 0;
    }
}

constexpr U64 moves(Square sq, PieceType p, U64 occupancy) {
    switch (p) {
        case PieceType::Knight: return KNIGHT_ATTACKS[sq];
        case PieceType::Bishop: return Magic::getBishopAttacks(sq, occupancy);
        case PieceType::Rook: return Magic::getRookAttacks(sq, occupancy);
        case PieceType::Queen: return Magic::getQueenAttacks(sq, occupancy);
        case PieceType::King: return KING_ATTACKS[sq];
        default: return 0;
    }
}

struct Debug {
    U64 bitboard;

    friend std::ostream& operator<<(std::ostream& os, const Debug& debug) {
        for (Rank rank = Rank::R8; rank >= Rank::R1; --rank) {
            os << " ";
            for (File file = File::F1; file <= File::F8; ++file) {
                U64 bitset = debug.bitboard & BB::set(makeSquare(file, rank));
                os << (bitset ? '1' : '.');
            }
            os << " " << rank << "\n";
        }

        os << " abcdefgh" << std::endl;
        return os;
    }
};

}  // namespace BB
