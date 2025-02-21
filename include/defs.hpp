#ifndef LATRUNCULI_DEFS_H
#define LATRUNCULI_DEFS_H

#include <array>
#include <sstream>
#include <string>
#include <vector>

#include "types.hpp"

inline constexpr Square sqFromCoords(const File file, const Rank rank) {
    return static_cast<Square>(rank * 8 + file);
}

inline Square sqFromString(const std::string& square) {
    auto file = File((int)square[0] - 'a');
    auto rank = Rank(((int)square[1] - '1'));
    return sqFromCoords(file, rank);
}

inline constexpr Rank rankOf(const Square square) { return Rank(square >> 3); }

inline constexpr Rank relativeRank(const Rank rank, const Color color) {
    return Rank(rank ^ (~color * 7));
}
inline constexpr Rank relativeRank(const Square square, const Color color) {
    return Rank(rankOf(square) ^ (~color * 7));
}

inline constexpr File fileOf(const Square square) { return File(square & 7); }

inline constexpr Piece makePiece(const Color c, const PieceType p) {
    // create piece from color+role
    return Piece((c << 3) | p);
}

inline constexpr PieceType pieceTypeOf(const Piece p) {
    // get the role from a piece
    return PieceType(p & 0x7);
}

inline constexpr Color pieceColorOf(const Piece p) {
    // get the color of a piece
    return Color(p >> 3);
}

template <Color c, PawnMove p, bool forward>
inline Square pawnMove(const Square sq) {
    return (forward == (c == WHITE)) ? Square(sq + static_cast<int>(p))
                                     : Square(sq - static_cast<int>(p));
}

template <PawnMove p, bool forward>
inline Square pawnMove(const Square sq, const Color c) {
    return (c == WHITE) ? pawnMove<WHITE, p, forward>(sq) : pawnMove<BLACK, p, forward>(sq);
}

static constexpr U64 _target(File file, Rank rank) {
    if (FILE1 <= file && file <= FILE8 && RANK1 <= rank && rank <= RANK8) {
        auto sq = rank * 8 + file;
        return 1ULL << sq;
    }
    return 0ULL;
}

constexpr U64 computeKnightAttacks(int sq) {
    U64 mask = 0;
    auto file = fileOf(Square(sq));
    auto rank = rankOf(Square(sq));
    mask |= _target(file + 2, rank + 1);
    mask |= _target(file + 2, rank - 1);
    mask |= _target(file - 2, rank + 1);
    mask |= _target(file - 2, rank - 1);
    mask |= _target(file + 1, rank + 2);
    mask |= _target(file - 1, rank + 2);
    mask |= _target(file + 1, rank - 2);
    mask |= _target(file - 1, rank - 2);
    return mask;
}

constexpr U64 computeKingAttacks(int sq) {
    U64 mask = 0;
    auto file = fileOf(Square(sq));
    auto rank = rankOf(Square(sq));
    mask |= _target(file - 1, rank - 1);
    mask |= _target(file - 1, rank + 1);
    mask |= _target(file + 1, rank - 1);
    mask |= _target(file + 1, rank + 1);
    mask |= _target(file, rank - 1);
    mask |= _target(file, rank + 1);
    mask |= _target(file - 1, rank);
    mask |= _target(file + 1, rank);
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

inline std::vector<std::string> splitStr(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(s);

    while (std::getline(ss, token, delim)) {
        tokens.push_back(token);
    }

    return tokens;
}

#endif