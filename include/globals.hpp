#ifndef LATRUNCULI_GLOBAL_H
#define LATRUNCULI_GLOBAL_H

#include <array>
#include <string>
#include <vector>
#include <sstream>

#include "types.hpp"

namespace G {

// clang-format off
constexpr U64 RANK_MASK[8] = {
    0xFF,
    0xFF00,
    0xFF0000,
    0xFF000000,
    0xFF00000000,
    0xFF0000000000,
    0xFF000000000000,
    0xFF00000000000000
};

constexpr U64 FILE_MASK[8] = {
    0x0101010101010101,
    0x202020202020202,
    0x404040404040404,
    0x808080808080808,
    0x1010101010101010,
    0x2020202020202020,
    0x4040404040404040,
    0x8080808080808080,
};
// clang-format on

const std::string STARTFEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

constexpr Rank RANK[2][8] = {
    {RANK8, RANK7, RANK6, RANK5, RANK4, RANK3, RANK2, RANK1},
    {RANK1, RANK2, RANK3, RANK4, RANK5, RANK6, RANK7, RANK8},
};

constexpr File FILE[2][8] = {
    {FILE8, FILE7, FILE6, FILE5, FILE4, FILE3, FILE2, FILE1},
    {FILE1, FILE2, FILE3, FILE4, FILE5, FILE6, FILE7, FILE8},
};

const U64 WHITESQUARES = 0x55AA55AA55AA55AA;
const U64 BLACKSQUARES = 0xAA55AA55AA55AA55;
const U64 WHITEHOLES   = 0x0000003CFFFF0000;
const U64 BLACKHOLES   = 0x0000FFFF3C000000;

const U64 CastlePathOO[2]          = { 0x6000000000000000ull, 0x0000000000000060ull };
const U64 CastlePathOOO[2]         = { 0x0E00000000000000ull, 0x000000000000000Eull };
const U64 KingCastlePathOO[2]      = { 0x7000000000000000ull, 0x0000000000000070ull };
const U64 KingCastlePathOOO[2]     = { 0x1C00000000000000ull, 0x000000000000001Cull };

const Square KingOrigin[2]        = { E8, E1 };
const Square KingDestinationOO[2] = { G8, G1 };
const Square KingDestinationOOO[2]= { C8, C1 };
const Square RookOriginOO[2]      = { H8, H1 };
const Square RookOriginOOO[2]     = { A8, A1 };

constexpr std::array<U64, NSQUARES> genBITSET() {
    std::array<U64, NSQUARES> arr = {};
    for (int i = 0; i < NSQUARES; ++i) {
        arr[i] = 1ull << i;
    }
    return arr;
}

constexpr std::array<U64, NSQUARES> genBITCLEAR() {
    std::array<U64, NSQUARES> arr = {};
    for (int i = 0; i < NSQUARES; ++i) {
        arr[i] = ~(1ull << i);
    }
    return arr;
}

constexpr std::array<std::array<int, NSQUARES>, NSQUARES> genDISTANCE() {
    std::array<std::array<int, NSQUARES>, NSQUARES> table = {};

    for (int sq1 = 0; sq1 < NSQUARES; ++sq1) {
        int rank1 = sq1 / 8, file1 = sq1 % 8;
        for (int sq2 = 0; sq2 < NSQUARES; ++sq2) {
            int rank2 = sq2 / 8, file2 = sq2 % 8;
            int rankDistance = abs(rank1 - rank2);
            int fileDistance = abs(file1 - file2);
            table[sq1][sq2] = std::max(rankDistance, fileDistance);
        }
    }

    return table;
}

constexpr std::array<std::array<U64, NSQUARES>, NSQUARES> genBITS_INLINE() {
    std::array<std::array<U64, NSQUARES>, NSQUARES> table = {};

    for (int sq1 = 0; sq1 < NSQUARES; ++sq1) {
        int rank1 = sq1 / 8, file1 = sq1 % 8;

        for (int sq2 = 0; sq2 < NSQUARES; ++sq2) {
            int rank2 = sq2 / 8, file2 = sq2 % 8;

            if (rank1 == rank2) {
                table[sq1][sq2] = RANK_MASK[rank1];
            } else if (file1 == file2) {
                table[sq1][sq2] = FILE_MASK[file1];
            } else if (int(rank1 - rank2) == int(file1 - file2)) {
                for (int k = 0; k < 8; ++k) {
                    int diag_rank = rank1 + k;
                    int diag_file = file1 + k;
                    if (diag_rank < 8 && diag_file < 8) {
                        table[sq1][sq2] |=
                            (1ULL << (diag_rank * 8 + diag_file));
                    }
                    diag_rank = rank1 - k;
                    diag_file = file1 - k;
                    if (diag_rank >= 0 && diag_file >= 0) {
                        table[sq1][sq2] |=
                            (1ULL << (diag_rank * 8 + diag_file));
                    }
                }
            } else if (rank1 + file1 == rank2 + file2) {
                for (int k = 0; k < 8; ++k) {
                    int diag_rank = rank1 + k;
                    int diag_file = file1 - k;
                    if (diag_rank < 8 && diag_file >= 0) {
                        table[sq1][sq2] |=
                            (1ULL << (diag_rank * 8 + diag_file));
                    }
                    diag_rank = rank1 - k;
                    diag_file = file1 + k;
                    if (diag_rank >= 0 && diag_file < 8) {
                        table[sq1][sq2] |=
                            (1ULL << (diag_rank * 8 + diag_file));
                    }
                }
            }
        }
    }

    return table;
}

constexpr std::array<std::array<U64, NSQUARES>, NSQUARES>
genBITS_BETWEEN() {
    std::array<std::array<U64, NSQUARES>, NSQUARES> table = {};

    for (int sq1 = 0; sq1 < NSQUARES; ++sq1) {
        int rank1 = sq1 / 8, file1 = sq1 % 8;

        for (int sq2 = 0; sq2 < NSQUARES; ++sq2) {
            int rank2 = sq2 / 8, file2 = sq2 % 8;

            if (rank1 == rank2) {
                int min_file = file1 < file2 ? file1 : file2;
                int max_file = file1 > file2 ? file1 : file2;
                for (int f = min_file + 1; f < max_file; ++f) {
                    table[sq1][sq2] |= (1ULL << (rank1 * 8 + f));
                }
            } else if (file1 == file2) {
                int min_rank = rank1 < rank2 ? rank1 : rank2;
                int max_rank = rank1 > rank2 ? rank1 : rank2;
                for (int r = min_rank + 1; r < max_rank; ++r) {
                    table[sq1][sq2] |= (1ULL << (r * 8 + file1));
                }
            } else if (int(rank1 - rank2) == int(file1 - file2)) {
                int min_sq = sq1 < sq2 ? sq1 : sq2;
                int max_sq = sq1 > sq2 ? sq1 : sq2;
                for (int s = min_sq + 9; s < max_sq; s += 9) {
                    table[sq1][sq2] |= (1ULL << s);
                }
            } else if (rank1 + file1 == rank2 + file2) {
                int min_sq = sq1 < sq2 ? sq1 : sq2;
                int max_sq = sq1 > sq2 ? sq1 : sq2;
                for (int s = min_sq + 7; s < max_sq; s += 7) {
                    table[sq1][sq2] |= (1ULL << s);
                }
            }
        }
    }

    return table;
}

constexpr std::array<U64, NSQUARES> BITSET = genBITSET();
constexpr std::array<U64, NSQUARES> BITCLEAR = genBITCLEAR();
constexpr std::array<std::array<int, NSQUARES>, NSQUARES> DISTANCE =
    genDISTANCE();
constexpr std::array<std::array<U64, NSQUARES>, NSQUARES> BITS_INLINE =
    genBITS_INLINE();
constexpr std::array<std::array<U64, NSQUARES>, NSQUARES> BITS_BETWEEN =
    genBITS_BETWEEN();

constexpr void addTarget(Square orig, File targetFile, Rank targetRank,
                         std::array<U64, 64>& arr) {
    auto square = Types::getSquare(targetFile, targetRank);
    if (Types::validFile(targetFile) && Types::validRank(targetRank)) {
        arr[orig] |= G::BITSET[square];
    }
}

constexpr std::array<U64, 64> genKNIGHT_ATTACKS() {
    std::array<U64, 64> arr = {};

    for (std::size_t i = 0; i < arr.size(); ++i) {
        Square sq = static_cast<Square>(i);
        auto file = Types::getFile(sq);
        auto rank = Types::getRank(sq);
        addTarget(sq, file + 2, rank + 1, arr);
        addTarget(sq, file + 2, rank - 1, arr);
        addTarget(sq, file - 2, rank + 1, arr);
        addTarget(sq, file - 2, rank - 1, arr);
        addTarget(sq, file + 1, rank + 2, arr);
        addTarget(sq, file - 1, rank + 2, arr);
        addTarget(sq, file + 1, rank - 2, arr);
        addTarget(sq, file - 1, rank - 2, arr);
    }
    return arr;
}

constexpr std::array<U64, 64> genKING_ATTACKS() {
    std::array<U64, 64> arr = {};

    for (std::size_t i = 0; i < arr.size(); ++i) {
        Square sq = static_cast<Square>(i);
        auto file = Types::getFile(sq);
        auto rank = Types::getRank(sq);
        addTarget(sq, file - 1, rank - 1, arr);
        addTarget(sq, file - 1, rank + 1, arr);
        addTarget(sq, file + 1, rank - 1, arr);
        addTarget(sq, file + 1, rank + 1, arr);
        addTarget(sq, file, rank - 1, arr);
        addTarget(sq, file, rank + 1, arr);
        addTarget(sq, file - 1, rank, arr);
        addTarget(sq, file + 1, rank, arr);
    }
    return arr;
}

constexpr std::array<U64, 64> KNIGHT_ATTACKS = genKNIGHT_ATTACKS();
constexpr std::array<U64, 64> KING_ATTACKS = genKING_ATTACKS();

inline U64 rankmask(Rank r, Color c) { return RANK_MASK[RANK[c][r]]; }

inline U64 filemask(File f, Color c) { return FILE_MASK[FILE[c][f]]; }

inline std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> tokens;
  std::string token;
  std::stringstream ss(s);

  while (std::getline(ss, token, delim)) {
    tokens.push_back(token);
  }

  return tokens;
}

}  // namespace G

#endif