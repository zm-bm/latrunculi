#ifndef LATRUNCULI_BITBOARD_H
#define LATRUNCULI_BITBOARD_H

#include <iostream>
#include <vector>
#include "types.hpp"
#include "move.hpp"
#include "globals.hpp"
#include "magics.hpp"

// A specialized bit array containing 64 single bit fields
// One bit for each square of a chess board
class BB {
    private:
        U64 bitboard;

    public:
        BB() = default;
        BB(U64 value)
        : bitboard(value) {}

        // Modifiers
        void clear(Square);
        void toggle(Square);
        void toggle(BB);

        // Accessors
        U64 get() const;
        bool isSet(Square) const;
        bool isEmpty() const;
        bool moreThanOneSet() const;
        Square lsb() const;
        Square msb() const;
        Square advanced(Color c) const;
        template<Color c> Square advanced() const;

        // Fill + span helpers
        BB getNorthFill() const;
        BB getSouthFill() const;
        BB getNorthSpan() const;
        BB getSouthSpan() const;
        BB getFill() const;
        BB getWestFill() const;
        BB getEastFill() const;

        template<Color> BB getFrontFill() const;
        template<Color> BB getBackFill() const;
        template<Color> BB getFrontSpan() const;
        template<Color> BB getBackSpan() const;
        template<Color> BB getFrontSpanWest() const;
        template<Color> BB getFrontSpanEast() const;
        template<Color> BB getBackSpanWest() const;
        template<Color> BB getBackSpanEast() const;
        template<Color> BB getFrontAttackSpan() const;
        template<Color> BB getAllFrontSpan() const;

        int count() const;
        int KernCount() const;

        // Shift operations
        inline BB shift_ea() const { return (*this << 1) & ~G::FILE_MASK[FILE1]; }
        inline BB shift_ne() const { return (*this << 9) & ~G::FILE_MASK[FILE1]; }
        inline BB shift_se() const { return (*this >> 7) & ~G::FILE_MASK[FILE1]; }
        inline BB shift_we() const { return (*this >> 1) & ~G::FILE_MASK[FILE8]; }
        inline BB shift_sw() const { return (*this >> 9) & ~G::FILE_MASK[FILE8]; }
        inline BB shift_nw() const { return (*this << 7) & ~G::FILE_MASK[FILE8]; }
        inline BB shift_so() const { return *this >> 8; }
        inline BB shift_no() const { return *this << 8; }

        // Operators
        inline operator bool() const { return bitboard; }
        inline operator U64() const { return bitboard; }
        friend std::ostream& operator<<(std::ostream&, const BB&);
        friend BB operator~(const BB&);
        friend BB operator&(const BB&, const BB&);
        friend BB operator|(const BB&, const BB&);
        friend BB operator^(const BB&, const BB&);
        friend BB operator&(const BB&, const U64&);
        friend BB operator|(const BB&, const U64&);
        friend BB operator^(const BB&, const U64&);
        friend BB operator&(const BB&, const Square&);
        friend BB operator|(const BB&, const Square&);
        friend BB operator^(const BB&, const Square&);
        friend BB operator<<(const BB& lhs, const int& rhs);
        friend BB operator>>(const BB& lhs, const int& rhs);
};

// BB operators
inline BB operator~(const BB& bitboard)
{ return BB(~bitboard.bitboard); }

inline BB operator&(const BB& lhs, const BB& rhs)
{ return BB(lhs.bitboard & rhs.bitboard); }

inline BB operator|(const BB& lhs, const BB& rhs)
{ return BB(lhs.bitboard | rhs.bitboard); }

inline BB operator^(const BB& lhs, const BB& rhs)
{ return BB(lhs.bitboard ^ rhs.bitboard); }

inline BB& operator&=(BB& lhs, BB rhs)
{ return lhs = BB(lhs & rhs); }

inline BB& operator|=(BB& lhs, BB rhs)
{ return lhs = BB(lhs | rhs); }

inline BB& operator^=(BB& lhs, BB rhs)
{ return lhs = BB(lhs ^ rhs); }

// U64 operators
inline BB operator&(const BB& lhs, const U64& rhs)
{ return BB(lhs.bitboard & rhs); }

inline BB operator|(const BB& lhs, const U64& rhs)
{ return BB(lhs.bitboard | rhs); }

inline BB operator^(const BB& lhs, const U64& rhs)
{ return BB(lhs.bitboard ^ rhs); }

inline BB& operator&=(BB& lhs, U64 rhs)
{ return lhs = BB(lhs & rhs); }

inline BB& operator|=(BB& lhs, U64 rhs)
{ return lhs = BB(lhs | rhs); }

inline BB& operator^=(BB& lhs, U64 rhs)
{ return lhs = BB(lhs ^ rhs); }

// Square operators
inline BB operator&(const BB& lhs, const Square& sq)
{ return BB(lhs & G::BITSET[sq]); }

inline BB operator|(const BB& lhs, const Square& sq)
{ return BB(lhs | G::BITSET[sq]); }

inline BB operator^(const BB& lhs, const Square& sq)
{ return BB(lhs ^ G::BITSET[sq]); }

// Shift operators
inline BB operator<<(const BB& lhs, const int& rhs)
{ return BB(lhs.bitboard << rhs); }

inline BB operator>>(const BB& lhs, const int& rhs)
{ return BB(lhs.bitboard >> rhs); }

inline void BB::clear(const Square sq)
{
    bitboard &= G::BITCLEAR[sq]; 
}

inline void BB::toggle(const Square sq)
{
    bitboard ^= G::BITSET[sq];
}

inline void BB::toggle(const BB targets)
{
    bitboard ^= targets.get();
}

inline U64 BB::get() const
{
    return bitboard;
}

inline bool BB::isSet(Square sq) const
{
    return bitboard & G::BITSET[sq];
}

inline bool BB::isEmpty() const
{
    return !(bool) bitboard;
}

inline bool BB::moreThanOneSet() const
{
    return bitboard & (bitboard - 1);
}

inline Square BB::lsb() const
{
    U64 idx;
	__asm__("bsfq %1, %0": "=r"(idx) : "rm"(bitboard));
    return (Square) idx;
}

inline Square BB::msb() const
{
    U64 idx;
	__asm__("bsrq %1, %0": "=r"(idx) : "rm"(bitboard));
    return (Square) idx;

}

inline Square BB::advanced(Color c) const
{
    if (c)
        return msb();
    else
        return lsb();
}

template<Color c>
inline Square BB::advanced() const
{
    if (c)
        return msb();
    else
        return lsb();
}

inline int BB::count() const
{
    return __builtin_popcountll(bitboard);
}

inline int BB::KernCount() const
{
    int count = 0;
    U64 bb = bitboard;

    while (bb) {
        count++;
        bb &= bb - 1; // reset LS1B
    }
    return count;
}


inline BB BB::getNorthFill() const
{
    U64 bb = bitboard;
    bb |= (bb <<  8);
    bb |= (bb << 16);
    bb |= (bb << 32);
    return bb;
}

inline BB BB::getSouthFill() const
{
    U64 bb = bitboard;
    bb |= (bb >>  8);
    bb |= (bb >> 16);
    bb |= (bb >> 32);
    return bb;
}

inline BB BB::getNorthSpan() const
{
    return getNorthFill().shift_no();
}

inline BB BB::getSouthSpan() const
{
    return getSouthFill().shift_so();
}

inline BB BB::getFill() const
{
    return getNorthFill() | getSouthFill();
}

inline BB BB::getWestFill() const
{
    return getFill().shift_we();
}

inline BB BB::getEastFill() const
{
    return getFill().shift_ea();
}

template<Color c>
inline BB BB::getFrontFill() const
{
    if (c)
        return getNorthFill();
    else
        return getSouthFill();
}

template<Color c>
inline BB BB::getBackFill() const
{
    if (c)
        return getSouthFill();
    else
        return getNorthFill();
}

template<Color c>
inline BB BB::getFrontSpan() const
{
    if (c)
        return getNorthSpan();
    else
        return getSouthSpan();
}

template<Color c>
inline BB BB::getBackSpan() const
{
    if (c)
        return getSouthSpan();
    else
        return getNorthSpan();
}

template<Color c>   
inline BB BB::getFrontSpanWest() const
{
    if (c)
        return getNorthSpan().shift_we();
    else
        return getSouthSpan().shift_we();
}

template<Color c>
inline BB BB::getFrontSpanEast() const
{
    if (c)
        return getNorthSpan().shift_ea();
    else
        return getSouthSpan().shift_ea();
}

template<Color c>
inline BB BB::getBackSpanWest() const
{
    if (c)
        return getSouthFill().shift_we();
    else
        return getNorthFill().shift_we();
}

template<Color c>
inline BB BB::getBackSpanEast() const
{
    if (c)
        return getSouthFill().shift_ea();
    else
        return getNorthFill().shift_ea();
}

template<Color c>
inline BB BB::getFrontAttackSpan() const
{
    return getFrontSpanWest<c>() | getFrontSpanEast<c>();
}

template<Color c>
inline BB BB::getAllFrontSpan() const
{
    return getFrontAttackSpan<c>() | getFrontSpan<c>();
}

/*
 * Friendly operators
 */

inline std::ostream& operator<<(std::ostream& os, const BB& bb)
{
    char output[64];

    for (unsigned i = 0; i < 64; i++) {
        if (bb.isSet(Square(i)))
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