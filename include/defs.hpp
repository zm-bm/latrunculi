#ifndef LATRUNCULI_DEFS_H
#define LATRUNCULI_DEFS_H

#include <array>
#include <sstream>
#include <string>
#include <vector>

#include "types.hpp"

namespace LtrnDefs {

constexpr U64 target(File file, Rank rank) {
    if (FILE1 <= file && file <= FILE8 && RANK1 <= rank && rank <= RANK8) {
        auto position = rank * 8 + file;
        return 1ULL << position;
    }
    return 0ULL;
}

constexpr U64 computeKnightAttacks(int sq) {
    U64 mask = 0;
    auto file = Types::getFile(Square(sq));
    auto rank = Types::getRank(Square(sq));
    mask |= target(file + 2, rank + 1);
    mask |= target(file + 2, rank - 1);
    mask |= target(file - 2, rank + 1);
    mask |= target(file - 2, rank - 1);
    mask |= target(file + 1, rank + 2);
    mask |= target(file - 1, rank + 2);
    mask |= target(file + 1, rank - 2);
    mask |= target(file - 1, rank - 2);
    return mask;
}

constexpr U64 computeKingAttacks(int sq) {
    U64 mask = 0;
    auto file = Types::getFile(Square(sq));
    auto rank = Types::getRank(Square(sq));
    mask |= target(file - 1, rank - 1);
    mask |= target(file - 1, rank + 1);
    mask |= target(file + 1, rank - 1);
    mask |= target(file + 1, rank + 1);
    mask |= target(file, rank - 1);
    mask |= target(file, rank + 1);
    mask |= target(file - 1, rank);
    mask |= target(file + 1, rank);
    return mask;
}

constexpr U64 computeDistance(int sq1, int sq2) {
    int rank1 = sq1 / 8, file1 = sq1 % 8;
    int rank2 = sq2 / 8, file2 = sq2 % 8;
    return std::max(abs(rank1 - rank2), abs(file1 - file2));
}

constexpr U64 computeBitsInline(int sq1, int sq2) {
    U64 mask = 0;
    int rank1 = sq1 / 8, file1 = sq1 % 8;
    int rank2 = sq2 / 8, file2 = sq2 % 8;

    if (rank1 == rank2) {
        mask = 0xffULL << (rank1 * 8);
    } else if (file1 == file2) {
        mask = 0x0101010101010101ULL << file1;
    } else {
        int rankDiff = rank1 - rank2;
        int fileDiff = file1 - file2;

        if (rankDiff == fileDiff) {
            // ↗ diagonal
            for (int i = 0; rank1 + i < 8 && file1 + i < 8; ++i) {
                mask |= (1ULL << ((rank1 + i) * 8 + (file1 + i)));
            }
            // ↙ diagonal
            for (int i = 0; rank1 + i >= 0 && file1 + i >= 0; --i) {
                mask |= (1ULL << ((rank1 + i) * 8 + (file1 + i)));
            }
        } else if (rank1 + file1 == rank2 + file2) {
            // ↘ diagonal
            for (int i = 0; rank1 + i < 8 && file1 - i >= 0; ++i) {
                mask |= (1ULL << ((rank1 + i) * 8 + (file1 - i)));
            }
            // ↖ diagonal
            for (int i = 0; rank1 + i >= 0 && file1 - i < 8; --i) {
                mask |= (1ULL << ((rank1 + i) * 8 + (file1 - i)));
            }
        }
    }

    return mask;
}

constexpr U64 computeBitsBetween(int sq1, int sq2) {
    U64 mask = 0;
    int rank1 = sq1 / 8, file1 = sq1 % 8;
    int rank2 = sq2 / 8, file2 = sq2 % 8;

    if (rank1 == rank2) {
        // Same rank
        int min_file = std::min(file1, file2);
        int max_file = std::max(file1, file2);
        for (int f = min_file + 1; f < max_file; ++f) {
            mask |= (1ULL << (rank1 * 8 + f));
        }
    } else if (file1 == file2) {
        // Same file
        int min_rank = std::min(rank1, rank2);
        int max_rank = std::max(rank1, rank2);
        for (int r = min_rank + 1; r < max_rank; ++r) {
            mask |= (1ULL << (r * 8 + file1));
        }
    } else if ((rank1 - rank2) == (file1 - file2)) {
        // Diagonal
        int min_sq = std::min(sq1, sq2);
        int max_sq = std::max(sq1, sq2);
        for (int s = min_sq + 9; s < max_sq; s += 9) {
            mask |= (1ULL << s);
        }
    } else if ((rank1 + file1) == (rank2 + file2)) {
        // Anti-diagonal
        int min_sq = std::min(sq1, sq2);
        int max_sq = std::max(sq1, sq2);
        for (int s = min_sq + 7; s < max_sq; s += 7) {
            mask |= (1ULL << s);
        }
    }

    return mask;
}

inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(s);

    while (std::getline(ss, token, delim)) {
        tokens.push_back(token);
    }

    return tokens;
}

}  // namespace LtrnDefs

#endif