#pragma once

#include <array>
#include <cstdint>

#include "core/bitboard.hpp"
#include "core/types.hpp"

enum Rank : std::int8_t { RANK1, RANK2, RANK3, RANK4, RANK5, RANK6, RANK7, RANK8 };

enum File : std::int8_t { FILE1, FILE2, FILE3, FILE4, FILE5, FILE6, FILE7, FILE8 };

enum Square : std::int8_t {
    // clang-format off
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    // clang-format on
    INVALID,
    N_SQUARES = 64
};

namespace square {

inline constexpr int north_east = 9;
inline constexpr int north      = 8;
inline constexpr int north_west = 7;
inline constexpr int east       = 1;
inline constexpr int west       = -1;
inline constexpr int south_east = -7;
inline constexpr int south      = -8;
inline constexpr int south_west = -9;

constexpr Square make(const File file, const Rank rank) {
    return static_cast<Square>((rank << 3) + file);
}

constexpr Square relative(const Square sq, const Color c) {
    return static_cast<Square>(sq ^ ((c - 1) & 63));
}

constexpr Rank rank_of(const Square square) {
    return static_cast<Rank>(square >> 3);
}

constexpr File file_of(const Square square) {
    return static_cast<File>(square & 7);
}

constexpr Rank relative_rank(const Square square, const Color color) {
    return static_cast<Rank>(rank_of(square) ^ (~color * 7));
}

// Compile-time builders for the distance, collinear, and between lookup tables.
namespace tables {

using SquareDistanceTable = std::array<std::array<std::uint8_t, N_SQUARES>, N_SQUARES>;
using SquareMaskTable     = std::array<std::array<Bitboard, N_SQUARES>, N_SQUARES>;

constexpr std::uint8_t distance_value(Square sq1, Square sq2) {
    const int file_diff = file_of(sq1) - file_of(sq2);
    const int rank_diff = rank_of(sq1) - rank_of(sq2);

    const std::uint8_t file_distance = std::uint8_t(file_diff < 0 ? -file_diff : file_diff);
    const std::uint8_t rank_distance = std::uint8_t(rank_diff < 0 ? -rank_diff : rank_diff);

    return (file_distance > rank_distance) ? file_distance : rank_distance;
}

constexpr SquareDistanceTable make_distance_table() {
    SquareDistanceTable table = {};
    for (int sq1 = A1; sq1 < N_SQUARES; ++sq1) {
        for (int sq2 = A1; sq2 < N_SQUARES; ++sq2)
            table[sq1][sq2] = distance_value(Square(sq1), Square(sq2));
    }
    return table;
}

constexpr Bitboard collinear_helper(int file, int rank, int file_delta, int rank_delta) {
    Bitboard mask = 0;
    while (0 <= rank && rank < 8 && 0 <= file && file < 8) {
        mask |= bb::set(make(File(file), Rank(rank)));
        file += file_delta;
        rank += rank_delta;
    }
    return mask;
}

constexpr Bitboard collinear_mask(Square sq1, Square sq2) {
    const int r1 = rank_of(sq1), r2 = rank_of(sq2);
    const int f1 = file_of(sq1), f2 = file_of(sq2);

    if (r1 == r2) {
        return bb::rank(Rank(r1));
    } else if (f1 == f2) {
        return bb::file(File(f1));
    } else if (r1 - r2 == f1 - f2) {
        // A1-H8 diagonal
        return collinear_helper(f1, r1, 1, 1) | collinear_helper(f1, r1, -1, -1);
    } else if (r1 + f1 == r2 + f2) {
        // H1-A8 diagonal
        return collinear_helper(f1, r1, -1, 1) | collinear_helper(f1, r1, 1, -1);
    }
    return 0;
}

constexpr SquareMaskTable make_collinear_table() {
    SquareMaskTable table = {};
    for (int sq1 = A1; sq1 < N_SQUARES; ++sq1) {
        for (int sq2 = A1; sq2 < N_SQUARES; ++sq2)
            table[sq1][sq2] = collinear_mask(Square(sq1), Square(sq2));
    }
    return table;
}

constexpr Bitboard between_helper(Square sq1, Square sq2, int delta) {
    const int min_sq = (sq1 < sq2) ? sq1 : sq2;
    const int max_sq = (sq1 < sq2) ? sq2 : sq1;

    Bitboard mask = 0;
    for (int sq = min_sq + delta; sq < max_sq; sq += delta)
        mask |= bb::set(Square(sq));
    return mask;
}

constexpr Bitboard between_mask(Square sq1, Square sq2) {
    const int r1 = rank_of(sq1), r2 = rank_of(sq2);
    const int f1 = file_of(sq1), f2 = file_of(sq2);

    if (r1 == r2) {
        return between_helper(sq1, sq2, square::east);
    } else if (f1 == f2) {
        return between_helper(sq1, sq2, square::north);
    } else if (r1 - r2 == f1 - f2) {
        return between_helper(sq1, sq2, square::north_east);
    } else if (r1 + f1 == r2 + f2) {
        return between_helper(sq1, sq2, square::north_west);
    }
    return 0;
}

constexpr SquareMaskTable make_between_table() {
    SquareMaskTable table = {};
    for (int sq1 = A1; sq1 < N_SQUARES; ++sq1) {
        for (int sq2 = A1; sq2 < N_SQUARES; ++sq2)
            table[sq1][sq2] = between_mask(Square(sq1), Square(sq2));
    }
    return table;
}

constexpr SquareDistanceTable distance_table  = make_distance_table();
constexpr SquareMaskTable     collinear_table = make_collinear_table();
constexpr SquareMaskTable     between_table   = make_between_table();

} // namespace tables

// Chebyshev distance: the number of king moves between two squares.
constexpr int distance(const Square sq1, const Square sq2) {
    return tables::distance_table[sq1][sq2];
}

// Bitboard containing the full rank, file, or diagonal shared by two squares.
constexpr Bitboard collinear(const Square sq1, const Square sq2) {
    return tables::collinear_table[sq1][sq2];
}

// Bitboard containing the squares strictly between two aligned squares.
constexpr Bitboard between(const Square sq1, const Square sq2) {
    return tables::between_table[sq1][sq2];
}

} // namespace square

constexpr Square operator+(Square sq1, int sq2) {
    return Square(int(sq1) + sq2);
}

constexpr Square operator-(Square sq1, int sq2) {
    return Square(int(sq1) - sq2);
}

constexpr Square& operator++(Square& sq) {
    return sq = Square(int(sq) + 1);
}

constexpr Square& operator--(Square& sq) {
    return sq = Square(int(sq) - 1);
}

constexpr Rank operator+(Rank r1, int r2) {
    return Rank(int(r1) + r2);
}

constexpr Rank operator-(Rank r1, int r2) {
    return Rank(int(r1) - r2);
}

constexpr Rank& operator++(Rank& r) {
    return r = r + 1;
}

constexpr Rank& operator--(Rank& r) {
    return r = r - 1;
}

constexpr File operator+(File f1, int f2) {
    return File(int(f1) + f2);
}

constexpr File operator-(File f1, int f2) {
    return File(int(f1) - f2);
}

constexpr File& operator++(File& f) {
    return f = f + 1;
}

constexpr File& operator--(File& f) {
    return f = f - 1;
}
