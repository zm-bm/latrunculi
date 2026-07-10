#include "core/notation.hpp"

#include "core/bitboard.hpp"

#include <iostream>

BitboardDebug debug_bitboard(uint64_t bitboard) {
    return {.bitboard = bitboard};
}

void print_bitboard(uint64_t bitboard) {
    std::cout << debug_bitboard(bitboard);
}

std::ostream& operator<<(std::ostream& os, BitboardDebug debug) {
    for (Rank rank = RANK8; rank >= RANK1; --rank) {
        os << " ";
        for (File file = FILE1; file <= FILE8; ++file) {
            const uint64_t bitset = debug.bitboard & bb::rank(rank) & bb::file(file);
            os << (bitset ? '1' : '.');
        }
        os << " " << to_char(rank) << "\n";
    }

    os << " abcdefgh" << std::endl;
    return os;
}
