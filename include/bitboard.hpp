#ifndef LATRUNCULI_BITBOARD_H
#define LATRUNCULI_BITBOARD_H

#include <iostream>
#include <vector>

#include "globals.hpp"
#include "types.hpp"

namespace BB {

// Defines a set of inline functions for manipulating bitboards.
// These functions operate on squares of a chessboard represented as U64 bit
// masks.

inline constexpr U64 set(const Square sq) {
    // Returns a bitboard with a single bit set corresponding to the given
    // square.
    return G::BITSET[sq];
}

inline constexpr U64 clear(const Square sq) {
    // Returns a bitboard with a single bit cleared corresponding to the given
    // square.
    return G::BITCLEAR[sq];
}

inline constexpr U64 bitsInline(const Square from, const Square to) {
    // Returns a bitboard representing all squares in a straight line
    // of the 'from' and 'to' squares.
    return G::BITS_INLINE[from][to];
}

inline constexpr U64 bitsBtwn(const Square from, const Square to) {
    // Returns a bitboard representing all squares between the 'from'
    // and 'to' squares, exclusive of the endpoints.
    return G::BITS_BETWEEN[from][to];
}

inline U64 moreThanOneSet(U64 bb) {
    // Returns non-zero if more than one bit is set in bb.
    return bb & (bb - 1);
}

inline int bitCount(U64 bb) {
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

inline Square advanced(Color c, U64 bb) {
    // Calls the appropriate advanced function (lsb or msb) based on the color.
    return advanced_fn[static_cast<Color>(c)](bb);
}

template <Color c>
inline Square advanced(U64 bb) {
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
    return (bb << 1) & ~G::FILE_MASK[FILE1];
}

inline U64 shiftNorthEast(U64 bb) {
    // Shifts the bitboard to the northeast (up-right) by 9 squares and masks
    // the invalid bits.
    return (bb << 9) & ~G::FILE_MASK[FILE1];
}

inline U64 shiftSouthEast(U64 bb) {
    // Shifts the bitboard to the southeast (down-right) by 7 squares and masks
    // the invalid bits.
    return (bb >> 7) & ~G::FILE_MASK[FILE1];
}

inline U64 shiftWest(U64 bb) {
    // Shifts the bitboard west (left) by 1 square and masks the invalid bits.
    return (bb >> 1) & ~G::FILE_MASK[FILE8];
}

inline U64 shiftSouthWest(U64 bb) {
    // Shifts the bitboard to the southwest (down-left) by 9 squares and masks
    // the invalid bits.
    return (bb >> 9) & ~G::FILE_MASK[FILE8];
}

inline U64 shiftNorthWest(U64 bb) {
    // Shifts the bitboard to the northwest (up-left) by 7 squares and masks the
    // invalid bits.
    return (bb << 7) & ~G::FILE_MASK[FILE8];
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
    return (c == WHITE) ? spanNorth(bb) : spanSouth(bb);
}

template <Color c>
inline U64 spanBack(U64 bb) {
    // Returns the span in the back direction for the given color.
    return (c == WHITE) ? spanSouth(bb) : spanNorth(bb);
}

template <Color c>
inline U64 getFrontAttackSpan(U64 bb) {
    // Returns the front attack span based on the given color's front span.
    U64 spanFrontBB = spanFront<c>(bb);
    return shiftWest(spanFrontBB) | shiftEast(spanFrontBB);
}

template <Color c>
inline U64 getAllFrontSpan(U64 bb) {
    // Returns all front span including attacks based on the given color's front
    // span.
    U64 spanFrontBB = spanFront<c>(bb);
    return shiftWest(spanFrontBB) | shiftEast(spanFrontBB) | spanFrontBB;
}

inline void print(std::ostream& os, const U64& bb) {
    // Prints the bitboard in a human-readable chessboard format.
    char output[64];

    // Generate visual representation of the bitboard.
    for (unsigned i = 0; i < 64; i++) {
        output[i] = (bb & G::BITSET[i]) ? '1' : '.';
    }

    os << "\n  abcdefgh\n";

    // Print each rank of the chessboard.
    for (int rank = 7; rank >= 0; rank--) {
        os << "  ";

        for (int file = 0; file < 8; file++) {
            os << output[Types::getSquare(File(file), Rank(rank))];
        }

        os << " " << (rank + 1) << "\n";
    }

    os << std::endl;
}

}  // namespace BB

#endif