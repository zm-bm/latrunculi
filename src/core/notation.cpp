#include "core/notation.hpp"

#include "core/bitboard.hpp"

#include <iostream>

// Wraps a bitboard for board-shaped stream output.
BitboardDebug debug_bitboard(Bitboard bitboard) {
    return {.bitboard = bitboard};
}

// Prints a board-shaped bitboard directly to stdout.
void print_bitboard(Bitboard bitboard) {
    std::cout << debug_bitboard(bitboard);
}

// Renders set bits from rank 8 to rank 1 for visual debugging.
std::ostream& operator<<(std::ostream& os, BitboardDebug debug) {
    for (Rank rank = RANK8; rank >= RANK1; --rank) {
        os << " ";
        for (File file = FILE1; file <= FILE8; ++file) {
            const Bitboard bitset = debug.bitboard & bb::rank(rank) & bb::file(file);
            os << (bitset ? '1' : '.');
        }
        os << " " << to_char(rank) << "\n";
    }

    os << " abcdefgh" << std::endl;
    return os;
}
