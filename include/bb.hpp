#pragma once

#include <array>
#include <iostream>
#include <vector>

#include "magics.hpp"
#include "types.hpp"
#include "defs.hpp"

namespace BB {

constexpr U64 RANK_MASK[N_RANKS] = {
    0x00000000000000FF,
    0x000000000000FF00,
    0x0000000000FF0000,
    0x00000000FF000000,
    0x000000FF00000000,
    0x0000FF0000000000,
    0x00FF000000000000,
    0xFF00000000000000,
};

constexpr U64 FILE_MASK[N_FILES] = {
    0x0101010101010101,
    0x0202020202020202,
    0x0404040404040404,
    0x0808080808080808,
    0x1010101010101010,
    0x2020202020202020,
    0x4040404040404040,
    0x8080808080808080,
};

constexpr std::array<U64, N_SQUARES> BITSET = [] {
    std::array<U64, N_SQUARES> arr = {};
    for (auto sq = 0; sq < N_SQUARES; ++sq) {
        arr[sq] = 1ull << sq;
    }
    return arr;
}();

constexpr std::array<U64, N_SQUARES> BITCLEAR = [] {
    std::array<U64, N_SQUARES> arr = {};
    for (auto sq = 0; sq < N_SQUARES; ++sq) {
        arr[sq] = ~(1ull << sq);
    }
    return arr;
}();

constexpr std::array<U64, N_SQUARES> KNIGHT_ATTACKS = [] {
    std::array<U64, N_SQUARES> arr = {};
    for (auto sq = 0; sq < N_SQUARES; ++sq) {
        arr[sq] = computeKnightAttacks(sq);
    }
    return arr;
}();

constexpr std::array<U64, N_SQUARES> KING_ATTACKS = [] {
    std::array<U64, N_SQUARES> arr = {};
    for (auto sq = 0; sq < N_SQUARES; ++sq) {
        arr[sq] = computeKingAttacks(sq);
    }
    return arr;
}();

constexpr std::array<std::array<U64, N_SQUARES>, N_SQUARES> BITS_INLINE = [] {
    std::array<std::array<U64, N_SQUARES>, N_SQUARES> table = {};
    for (auto sq1 = 0; sq1 < N_SQUARES; ++sq1) {
        for (int sq2 = 0; sq2 < N_SQUARES; ++sq2) {
            table[sq1][sq2] = computeBitsInline(sq1, sq2);
        }
    }
    return table;
}();

constexpr std::array<std::array<U64, N_SQUARES>, N_SQUARES> BITS_BETWEEN = [] {
    std::array<std::array<U64, N_SQUARES>, N_SQUARES> table = {};
    for (auto sq1 = 0; sq1 < N_SQUARES; ++sq1) {
        for (int sq2 = 0; sq2 < N_SQUARES; ++sq2) {
            table[sq1][sq2] = computeBitsBetween(sq1, sq2);
        }
    }
    return table;
}();

constexpr std::array<std::array<int, N_SQUARES>, N_SQUARES> DISTANCE = [] {
    std::array<std::array<int, N_SQUARES>, N_SQUARES> table = {};
    for (int sq1 = 0; sq1 < N_SQUARES; ++sq1) {
        for (int sq2 = 0; sq2 < N_SQUARES; ++sq2) {
            table[sq1][sq2] = computeDistance(sq1, sq2);
        }
    }
    return table;
}();

const U64 CastlePathOO[N_COLORS] = {0x6000000000000000ull, 0x0000000000000060ull};
const U64 CastlePathOOO[N_COLORS] = {0x0E00000000000000ull, 0x000000000000000Eull};
const U64 KingCastlePathOO[N_COLORS] = {0x7000000000000000ull, 0x0000000000000070ull};
const U64 KingCastlePathOOO[N_COLORS] = {0x1C00000000000000ull, 0x000000000000001Cull};

constexpr Rank RANK_BY_COLOR[N_COLORS][N_RANKS] = {
    {RANK8, RANK7, RANK6, RANK5, RANK4, RANK3, RANK2, RANK1},
    {RANK1, RANK2, RANK3, RANK4, RANK5, RANK6, RANK7, RANK8},
};
inline U64 rank(Rank r, Color c) { return RANK_MASK[RANK_BY_COLOR[c][r]]; }
inline U64 rank(Rank r) { return RANK_MASK[RANK_BY_COLOR[WHITE][r]]; }

constexpr File FILE_BY_COLOR[N_COLORS][N_FILES] = {
    {FILE8, FILE7, FILE6, FILE5, FILE4, FILE3, FILE2, FILE1},
    {FILE1, FILE2, FILE3, FILE4, FILE5, FILE6, FILE7, FILE8},
};
inline U64 file(File f) { return FILE_MASK[FILE_BY_COLOR[WHITE][f]]; }
inline U64 file(File f, Color c) { return FILE_MASK[FILE_BY_COLOR[c][f]]; }

inline constexpr U64 set(const Square sq) {
    // Returns a bitboard with a single bit set corresponding to the given square.
    return BB::BITSET[sq];
}

inline constexpr U64 clear(const Square sq) {
    // Returns a bitboard with a single bit cleared corresponding to the given square.
    return BB::BITCLEAR[sq];
}

inline constexpr U64 inlineBB(const Square from, const Square to) {
    // Returns a bitboard representing all squares in a straight line
    // of the 'from' and 'to' squares.
    return BITS_INLINE[from][to];
}

inline constexpr U64 betweenBB(const Square from, const Square to) {
    // Returns a bitboard representing all squares between the 'from'
    // and 'to' squares, exclusive of the endpoints.
    return BITS_BETWEEN[from][to];
}

inline U64 hasMoreThanOne(U64 bb) {
    // Returns non-zero if more than one bit is set in bb.
    return bb & (bb - 1);
}

inline int count(U64 bb) {
    // Returns the count of set bits (1's) in bb.
    return __builtin_popcountll(bb);
}

inline Square lsb(U64 bb) {
    // Returns the index of the least significant bit set in bb.
    return static_cast<Square>(__builtin_ctzll(bb));
}

inline Square msb(U64 bb) {
    // Returns the index of the most significant bit set in bb.
    return static_cast<Square>(63 - __builtin_clzll(bb));
}

inline Square (*const advanced_fn[2])(U64) = {lsb, msb};

inline Square advancedSq(Color c, U64 bb) {
    // Calls the appropriate advanced function (lsb or msb) based on the color.
    return advanced_fn[static_cast<Color>(c)](bb);
}

template <Color c>
inline Square advancedSq(U64 bb) {
    // Calls the appropriate advanced function (lsb or msb) based on the color.
    return advanced_fn[static_cast<Color>(c)](bb);
}

inline U64 fillNorth(U64 bb) {
    // Fills the squares north of bb by shifting and OR'ing the bits.
    bb |= (bb << 8);
    bb |= (bb << 16);
    bb |= (bb << 32);
    return bb;
}

inline U64 fillSouth(U64 bb) {
    // Fills the squares south of bb by shifting and OR'ing the bits.
    bb |= (bb >> 8);
    bb |= (bb >> 16);
    bb |= (bb >> 32);
    return bb;
}

inline U64 fillFiles(U64 bb) {
    // Fills both north and south for the given bitboard.
    return fillNorth(bb) | fillSouth(bb);
}
inline U64 shiftSouth(U64 bb) {
    // Shifts the bitboard south (down) by 8 squares.
    return bb >> 8;
}
inline U64 shiftNorth(U64 bb) {
    // Shifts the bitboard north (up) by 8 squares.
    return bb << 8;
}

inline U64 shiftEast(U64 bb) {
    // Shifts the bitboard east (right) by 1 square and masks the invalid bits.
    return (bb << 1) & ~BB::FILE_MASK[FILE1];
}

inline U64 shiftNorthEast(U64 bb) {
    // Shifts the bitboard to the northeast (up-right) by 9 squares and masks
    // the invalid bits.
    return (bb << 9) & ~BB::FILE_MASK[FILE1];
}

inline U64 shiftSouthEast(U64 bb) {
    // Shifts the bitboard to the southeast (down-right) by 7 squares and masks
    // the invalid bits.
    return (bb >> 7) & ~BB::FILE_MASK[FILE1];
}

inline U64 shiftWest(U64 bb) {
    // Shifts the bitboard west (left) by 1 square and masks the invalid bits.
    return (bb >> 1) & ~BB::FILE_MASK[FILE8];
}

inline U64 shiftSouthWest(U64 bb) {
    // Shifts the bitboard to the southwest (down-left) by 9 squares and masks
    // the invalid bits.
    return (bb >> 9) & ~BB::FILE_MASK[FILE8];
}

inline U64 shiftNorthWest(U64 bb) {
    // Shifts the bitboard to the northwest (up-left) by 7 squares and masks the
    // invalid bits.
    return (bb << 7) & ~BB::FILE_MASK[FILE8];
}
inline U64 spanNorth(U64 bb) {
    // Fills and shifts the bitboard north to create a span.
    return shiftNorth(fillNorth(bb));
}

inline U64 spanSouth(U64 bb) {
    // Fills and shifts the bitboard south to create a span.
    return shiftSouth(fillSouth(bb));
}

template <Color c>
inline U64 spanFront(U64 bb) {
    // Returns the span in the front direction for the given color.
    if constexpr (c == WHITE) {
        return spanNorth(bb);
    } else {
        return spanSouth(bb);
    }
}

template <Color c>
inline U64 spanBack(U64 bb) {
    // Returns the span in the back direction for the given color.
    if constexpr (c == WHITE) {
        return spanSouth(bb);
    } else {
        return spanNorth(bb);
    }
}

template <Color c>
inline U64 pawnAttackSpan(U64 bb) {
    // Returns the attack span for pawns, covering the diagonal squares in front
    // of the bitboard to the left and right, based on the color.
    U64 spanFrontBB = spanFront<c>(bb);
    return shiftWest(spanFrontBB) | shiftEast(spanFrontBB);
}

template <Color c>
inline U64 pawnFullSpan(U64 bb) {
    // Returns the full pawn span, including both the forward moves and attacks
    // to the left and right diagonals, based on the color.
    U64 spanFrontBB = spanFront<c>(bb);
    return shiftWest(spanFrontBB) | shiftEast(spanFrontBB) | spanFrontBB;
}

template <Color c>
inline U64 kingShield(Square sq) {
    U64 bitboard = BB::set(sq);
    if constexpr (c == WHITE) {
        return (BB::shiftNorthWest(bitboard) | BB::shiftNorth(bitboard) | BB::shiftNorthEast(bitboard));
    } else {
        return (BB::shiftSouthWest(bitboard) | BB::shiftSouth(bitboard) | BB::shiftSouthEast(bitboard));
    }
}

template <PawnMove p, Color c>
inline U64 pawnMoves(U64 pawns) {
    if constexpr (p == LEFT) {
        pawns &= ~file(FILE1, c);
    } else if constexpr (p == RIGHT) {
        pawns &= ~file(FILE8, c);
    }

    if constexpr (c == WHITE) {
        return pawns << static_cast<int>(p);
    } else {
        return pawns >> static_cast<int>(p);
    }
}

template <PawnMove p>
inline U64 pawnMoves(U64 pawns, Color c) {
    if (c == WHITE) {
        return pawnMoves<p, WHITE>(pawns);
    } else {
        return pawnMoves<p, BLACK>(pawns);
    }
};

template <Color c>
inline U64 pawnAttacks(U64 pawns) {
    return pawnMoves<LEFT, c>(pawns) | pawnMoves<RIGHT, c>(pawns);
}

inline U64 pawnAttacks(U64 pawns, Color c) {
    return pawnMoves<LEFT>(pawns, c) | pawnMoves<RIGHT>(pawns, c);
}

template <PieceType p>
inline U64 pieceMoves(Square sq, U64 occupancy) {
    switch (p) {
        case KNIGHT: return KNIGHT_ATTACKS[sq];
        case BISHOP: return Magics::getBishopAttacks(sq, occupancy);
        case ROOK: return Magics::getRookAttacks(sq, occupancy);
        case QUEEN: return Magics::getQueenAttacks(sq, occupancy);
        case KING: return KING_ATTACKS[sq];
    }
}

template <PieceType p>
inline U64 pieceMoves(Square sq) {
    return pieceMoves<p>(sq, 0);
}

inline U64 pieceMoves(Square sq, PieceType p, U64 occupancy) {
    switch (p) {
        case KNIGHT: return pieceMoves<KNIGHT>(sq, occupancy);
        case BISHOP: return pieceMoves<BISHOP>(sq, occupancy);
        case ROOK: return pieceMoves<ROOK>(sq, occupancy);
        case QUEEN: return pieceMoves<QUEEN>(sq, occupancy);
        case KING: return pieceMoves<KING>(sq, occupancy);
        default: return 0;
    }
}

inline U64 pieceMoves(Square sq, PieceType p) { return pieceMoves(sq, p, 0); }

struct Bitboard {
    U64 value;

    friend std::ostream& operator<<(std::ostream& os, const Bitboard& bb) {
        char output[64];

        // Generate visual representation of the bitboard.
        for (unsigned i = 0; i < 64; i++) {
                output[i] = (bb.value & BB::BITSET[i]) ? '1' : '.';
        }

        os << "\n  abcdefgh\n";

        // Print each rank of the chessboard.
        for (int rank = 7; rank >= 0; rank--) {
            os << "  ";

            for (int file = 0; file < 8; file++) {
                os << output[makeSquare(File(file), Rank(rank))];
            }

            os << " " << (rank + 1) << "\n";
        }

        os << std::endl;
            return os;
    }
};

}  // namespace BB
