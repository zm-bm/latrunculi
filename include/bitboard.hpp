#ifndef LATRUNCULI_BITBOARD_H
#define LATRUNCULI_BITBOARD_H

#include <iostream>
#include <vector>
#include "types.hpp"
#include "globals.hpp"

namespace BB {

inline U64 set(const Square sq) { return G::BITSET[sq]; }
inline U64 clear(const Square sq) { return G::BITCLEAR[sq]; }
inline U64 bInline(const Square from, const Square to) { return G::BITS_INLINE[from][to]; }

// inline void clear(U64& bb, const Square sq) { bb &= G::BITCLEAR[sq]; }
// inline void toggle(U64& bb, const Square sq) { bb ^= G::BITSET[sq]; }
// inline void toggle(U64& bb1, const U64& bb2) { bb1 ^= bb2; }

inline constexpr U64 moreThanOneSet(U64 bb) { return bb & (bb - 1); }
inline int bitCount(U64 bb) { return __builtin_popcountll(bb); }

inline Square lsb(U64 bb)
{
    U64 idx;
	__asm__("bsfq %1, %0": "=r"(idx) : "rm"(bb));
    return (Square) idx;
}

inline Square msb(U64 bb)
{
    U64 idx;
	__asm__("bsrq %1, %0": "=r"(idx) : "rm"(bb));
    return (Square) idx;

}

inline Square advanced(Color c, U64 bb)
{
    if (c)
        return msb(bb);
    else
        return lsb(bb);
}

template<Color c>
inline Square advanced(U64 bb)
{
    if (c)
        return msb(bb);
    else
        return lsb(bb);
}

inline U64 getNorthFill(U64 bb)
{
    bb |= (bb <<  8);
    bb |= (bb << 16);
    bb |= (bb << 32);
    return bb;
}

inline U64 getSouthFill(U64 bb)
{
    bb |= (bb >>  8);
    bb |= (bb >> 16);
    bb |= (bb >> 32);
    return bb;
}

inline U64 shift_ea(U64 bb) { return (bb << 1) & ~G::FILE_MASK[FILE1]; }
inline U64 shift_ne(U64 bb) { return (bb << 9) & ~G::FILE_MASK[FILE1]; }
inline U64 shift_se(U64 bb) { return (bb >> 7) & ~G::FILE_MASK[FILE1]; }
inline U64 shift_we(U64 bb) { return (bb >> 1) & ~G::FILE_MASK[FILE8]; }
inline U64 shift_sw(U64 bb) { return (bb >> 9) & ~G::FILE_MASK[FILE8]; }
inline U64 shift_nw(U64 bb) { return (bb << 7) & ~G::FILE_MASK[FILE8]; }
inline U64 shift_so(U64 bb) { return bb >> 8; }
inline U64 shift_no(U64 bb) { return bb << 8; }

inline U64 getNorthSpan(U64 bb)
{
    return shift_no(getNorthFill(bb));
}

inline U64 getSouthSpan(U64 bb)
{
    return shift_so(getSouthFill(bb));
}

inline U64 getFill(U64 bb)
{
    return getNorthFill(bb) | getSouthFill(bb);
}

inline U64 getWestFill(U64 bb)
{
    return shift_we(getFill(bb));
}

inline U64 getEastFill(U64 bb)
{
    return shift_ea(getFill(bb));
}

template<Color c>
inline U64 getFrontFill(U64 bb)
{
    if (c)
        return getNorthFill(bb);
    else
        return getSouthFill(bb);
}

template<Color c>
inline U64 getBackFill(U64 bb)
{
    if (c)
        return getSouthFill(bb);
    else
        return getNorthFill(bb);
}

template<Color c>
inline U64 getFrontSpan(U64 bb)
{
    if (c)
        return getNorthSpan(bb);
    else
        return getSouthSpan(bb);
}

template<Color c>
inline U64 getBackSpan(U64 bb)
{
    if (c)
        return getSouthSpan(bb);
    else
        return getNorthSpan(bb);
}

template<Color c>   
inline U64 getFrontSpanWest(U64 bb)
{
    if (c)
        return shift_we(getNorthSpan(bb));
    else
        return shift_we(getSouthSpan(bb));
}

template<Color c>
inline U64 getFrontSpanEast(U64 bb)
{
    if (c)
        return shift_ea(getNorthSpan(bb));
    else
        return shift_ea(getSouthSpan(bb));
}

template<Color c>
inline U64 getBackSpanWest(U64 bb)
{
    if (c)
        return shift_we(getSouthFill(bb));
    else
        return shift_we(getNorthFill(bb));
}

template<Color c>
inline U64 getBackSpanEast(U64 bb)
{
    if (c)
        return shift_ea(getSouthFill(bb));
    else
        return shift_ea(getNorthFill(bb));
}

template<Color c>
inline U64 getFrontAttackSpan(U64 bb)
{
    return getFrontSpanWest<c>(bb) | getFrontSpanEast<c>(bb);
}

template<Color c>
inline U64 getAllFrontSpan(U64 bb)
{
    return getFrontAttackSpan<c>(bb) | getFrontSpan<c>(bb);
}

inline std::ostream& print(std::ostream& os, const U64& bb)
{
    char output[64];

    for (unsigned i = 0; i < 64; i++) {
        if (bb & G::BITSET[i])
            output[i] = '1';
        else
            output[i] = '.';
    }

    os << std::endl << "  abcdefgh" << std::endl;

    for (int rank = 7; rank >= 0; rank--) {
        os << "  ";

        for (int file = 0; file < 8; file++) {
            os << output[Types::getSquare(File(file), Rank(rank))];
        }

        os << " " << rank+1 << std::endl;
    }

    os << std::endl;

    return os;
}


} // namespace BB

#endif