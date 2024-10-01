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

} // namespace BB

// A specialized bit array containing 64 single bit fields
// One bit for each square of a chess board
class BBz {
    private:
        U64 bitboard;

    public:
        constexpr BBz() noexcept : bitboard(0) {}
        constexpr BBz(U64 value) noexcept : bitboard(value) {}

        // Modifiers
        inline void clear(const Square sq) noexcept { bitboard &= G::BITCLEAR[sq]; }
        inline void toggle(const Square sq) noexcept { bitboard ^= G::BITSET[sq]; }
        inline void toggle(const BBz& other) noexcept { bitboard ^= other.bitboard; }

        // Accessors
        bool moreThanOneSet() const;
        Square lsb() const;
        Square msb() const;
        Square advanced(Color c) const;
        template<Color c> Square advanced() const;

        // Fill + span helpers
        BBz getNorthFill() const;
        BBz getSouthFill() const;
        BBz getNorthSpan() const;
        BBz getSouthSpan() const;
        BBz getFill() const;
        BBz getWestFill() const;
        BBz getEastFill() const;

        template<Color> BBz getFrontFill() const;
        template<Color> BBz getBackFill() const;
        template<Color> BBz getFrontSpan() const;
        template<Color> BBz getBackSpan() const;
        template<Color> BBz getFrontSpanWest() const;
        template<Color> BBz getFrontSpanEast() const;
        template<Color> BBz getBackSpanWest() const;
        template<Color> BBz getBackSpanEast() const;
        template<Color> BBz getFrontAttackSpan() const;
        template<Color> BBz getAllFrontSpan() const;

        int count() const;
        int KernCount() const;

        // Shift operations
        inline BBz shift_ea() const { return (*this << 1) & ~G::FILE_MASK[FILE1]; }
        inline BBz shift_ne() const { return (*this << 9) & ~G::FILE_MASK[FILE1]; }
        inline BBz shift_se() const { return (*this >> 7) & ~G::FILE_MASK[FILE1]; }
        inline BBz shift_we() const { return (*this >> 1) & ~G::FILE_MASK[FILE8]; }
        inline BBz shift_sw() const { return (*this >> 9) & ~G::FILE_MASK[FILE8]; }
        inline BBz shift_nw() const { return (*this << 7) & ~G::FILE_MASK[FILE8]; }
        inline BBz shift_so() const { return *this >> 8; }
        inline BBz shift_no() const { return *this << 8; }

        // Operators
        inline operator bool() const noexcept { return bitboard; }
        inline operator U64() const noexcept { return bitboard; }
        friend std::ostream& operator<<(std::ostream&, const BBz&);
        friend BBz operator~(const BBz&);
        friend BBz operator&(const BBz&, const BBz&);
        friend BBz operator|(const BBz&, const BBz&);
        friend BBz operator^(const BBz&, const BBz&);
        friend BBz operator&(const BBz&, const U64&);
        friend BBz operator|(const BBz&, const U64&);
        friend BBz operator^(const BBz&, const U64&);
        friend BBz operator&(const BBz&, const Square&);
        friend BBz operator|(const BBz&, const Square&);
        friend BBz operator^(const BBz&, const Square&);
        friend BBz operator<<(const BBz& lhs, const int& rhs);
        friend BBz operator>>(const BBz& lhs, const int& rhs);
        inline bool operator==(const BBz& other) const noexcept { return bitboard == other.bitboard; }
};

// BBz operators
inline BBz operator~(const BBz& bitboard)
{ return BBz(~bitboard.bitboard); }

inline BBz operator&(const BBz& lhs, const BBz& rhs)
{ return BBz(lhs.bitboard & rhs.bitboard); }

inline BBz operator|(const BBz& lhs, const BBz& rhs)
{ return BBz(lhs.bitboard | rhs.bitboard); }

inline BBz operator^(const BBz& lhs, const BBz& rhs)
{ return BBz(lhs.bitboard ^ rhs.bitboard); }

inline BBz& operator&=(BBz& lhs, BBz rhs)
{ return lhs = BBz(lhs & rhs); }

inline BBz& operator|=(BBz& lhs, BBz rhs)
{ return lhs = BBz(lhs | rhs); }

inline BBz& operator^=(BBz& lhs, BBz rhs)
{ return lhs = BBz(lhs ^ rhs); }

// U64 operators
inline BBz operator&(const BBz& lhs, const U64& rhs)
{ return BBz(lhs.bitboard & rhs); }

inline BBz operator|(const BBz& lhs, const U64& rhs)
{ return BBz(lhs.bitboard | rhs); }

inline BBz operator^(const BBz& lhs, const U64& rhs)
{ return BBz(lhs.bitboard ^ rhs); }

inline BBz& operator&=(BBz& lhs, U64 rhs)
{ return lhs = BBz(lhs & rhs); }

inline BBz& operator|=(BBz& lhs, U64 rhs)
{ return lhs = BBz(lhs | rhs); }

inline BBz& operator^=(BBz& lhs, U64 rhs)
{ return lhs = BBz(lhs ^ rhs); }

// Square operators
inline BBz operator&(const BBz& lhs, const Square& sq)
{ return BBz(lhs & G::BITSET[sq]); }

inline BBz operator|(const BBz& lhs, const Square& sq)
{ return BBz(lhs | G::BITSET[sq]); }

inline BBz operator^(const BBz& lhs, const Square& sq)
{ return BBz(lhs ^ G::BITSET[sq]); }

// Shift operators
inline BBz operator<<(const BBz& lhs, const int& rhs)
{ return BBz(lhs.bitboard << rhs); }

inline BBz operator>>(const BBz& lhs, const int& rhs)
{ return BBz(lhs.bitboard >> rhs); }

inline bool BBz::moreThanOneSet() const
{
    return bitboard & (bitboard - 1);
}

inline Square BBz::lsb() const
{
    U64 idx;
	__asm__("bsfq %1, %0": "=r"(idx) : "rm"(bitboard));
    return (Square) idx;
}

inline Square BBz::msb() const
{
    U64 idx;
	__asm__("bsrq %1, %0": "=r"(idx) : "rm"(bitboard));
    return (Square) idx;

}

inline Square BBz::advanced(Color c) const
{
    if (c)
        return msb();
    else
        return lsb();
}

template<Color c>
inline Square BBz::advanced() const
{
    if (c)
        return msb();
    else
        return lsb();
}

inline int BBz::count() const
{
    return __builtin_popcountll(bitboard);
}

inline int BBz::KernCount() const
{
    int count = 0;
    U64 bb = bitboard;

    while (bb) {
        count++;
        bb &= bb - 1; // reset LS1B
    }
    return count;
}


inline BBz BBz::getNorthFill() const
{
    U64 bb = bitboard;
    bb |= (bb <<  8);
    bb |= (bb << 16);
    bb |= (bb << 32);
    return bb;
}

inline BBz BBz::getSouthFill() const
{
    U64 bb = bitboard;
    bb |= (bb >>  8);
    bb |= (bb >> 16);
    bb |= (bb >> 32);
    return bb;
}

inline BBz BBz::getNorthSpan() const
{
    return getNorthFill().shift_no();
}

inline BBz BBz::getSouthSpan() const
{
    return getSouthFill().shift_so();
}

inline BBz BBz::getFill() const
{
    return getNorthFill() | getSouthFill();
}

inline BBz BBz::getWestFill() const
{
    return getFill().shift_we();
}

inline BBz BBz::getEastFill() const
{
    return getFill().shift_ea();
}

template<Color c>
inline BBz BBz::getFrontFill() const
{
    if (c)
        return getNorthFill();
    else
        return getSouthFill();
}

template<Color c>
inline BBz BBz::getBackFill() const
{
    if (c)
        return getSouthFill();
    else
        return getNorthFill();
}

template<Color c>
inline BBz BBz::getFrontSpan() const
{
    if (c)
        return getNorthSpan();
    else
        return getSouthSpan();
}

template<Color c>
inline BBz BBz::getBackSpan() const
{
    if (c)
        return getSouthSpan();
    else
        return getNorthSpan();
}

template<Color c>   
inline BBz BBz::getFrontSpanWest() const
{
    if (c)
        return getNorthSpan().shift_we();
    else
        return getSouthSpan().shift_we();
}

template<Color c>
inline BBz BBz::getFrontSpanEast() const
{
    if (c)
        return getNorthSpan().shift_ea();
    else
        return getSouthSpan().shift_ea();
}

template<Color c>
inline BBz BBz::getBackSpanWest() const
{
    if (c)
        return getSouthFill().shift_we();
    else
        return getNorthFill().shift_we();
}

template<Color c>
inline BBz BBz::getBackSpanEast() const
{
    if (c)
        return getSouthFill().shift_ea();
    else
        return getNorthFill().shift_ea();
}

template<Color c>
inline BBz BBz::getFrontAttackSpan() const
{
    return getFrontSpanWest<c>() | getFrontSpanEast<c>();
}

template<Color c>
inline BBz BBz::getAllFrontSpan() const
{
    return getFrontAttackSpan<c>() | getFrontSpan<c>();
}

/*
 * Friendly operators
 */

inline std::ostream& operator<<(std::ostream& os, const BBz& bb)
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

#endif
