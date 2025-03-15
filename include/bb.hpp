#pragma once

#include <array>
#include <iostream>
#include <vector>

#include "magics.hpp"
#include "types.hpp"

namespace BB {

using BitboardTable  = std::array<U64, N_SQUARES>;
using BitboardMatrix = std::array<std::array<U64, N_SQUARES>, N_SQUARES>;

constexpr std::array<U64, N_RANKS> RANK_MASK = [] {
    std::array<U64, N_RANKS> arr = {};
    for (int i = 0; i < N_RANKS; ++i) {
        arr[i] = 0xFFULL << (8 * i);
    }
    return arr;
}();

constexpr std::array<U64, N_FILES> FILE_MASK = [] {
    std::array<U64, N_FILES> arr = {};
    for (int i = 0; i < N_FILES; ++i) {
        arr[i] = 0x0101010101010101ULL << i;
    }
    return arr;
}();

constexpr BitboardTable BITSET = [] {
    BitboardTable arr = {};
    for (auto sq = 0; sq < N_SQUARES; ++sq) {
        arr[sq] = 1ull << sq;
    }
    return arr;
}();

constexpr BitboardTable BITCLEAR = [] {
    BitboardTable arr = {};
    for (auto sq = 0; sq < N_SQUARES; ++sq) {
        arr[sq] = ~(1ull << sq);
    }
    return arr;
}();

constexpr BitboardMatrix DISTANCE = [] {
    BitboardMatrix table = {};
    for (int sq1 = 0; sq1 < N_SQUARES; ++sq1) {
        for (int sq2 = 0; sq2 < N_SQUARES; ++sq2) {
            int rank1 = sq1 / 8, file1 = sq1 % 8;
            int rank2 = sq2 / 8, file2 = sq2 % 8;
            table[sq1][sq2] = std::max(abs(rank1 - rank2), abs(file1 - file2));
        }
    }
    return table;
}();

static constexpr U64 addSquare(File file, Rank rank) {
    return (FILE1 <= file && file <= FILE8 && RANK1 <= rank && rank <= RANK8)
               ? (1ULL << makeSquare(file, rank))
               : 0ULL;
}

constexpr BitboardTable KNIGHT_ATTACKS = [] {
    BitboardTable table = {};
    for (auto sq = 0; sq < N_SQUARES; ++sq) {
        auto file  = fileOf(Square(sq));
        auto rank  = rankOf(Square(sq));
        table[sq]  = 0;
        table[sq] |= addSquare(file + 2, rank + 1);
        table[sq] |= addSquare(file + 2, rank - 1);
        table[sq] |= addSquare(file - 2, rank + 1);
        table[sq] |= addSquare(file - 2, rank - 1);
        table[sq] |= addSquare(file + 1, rank + 2);
        table[sq] |= addSquare(file - 1, rank + 2);
        table[sq] |= addSquare(file + 1, rank - 2);
        table[sq] |= addSquare(file - 1, rank - 2);
    }
    return table;
}();

constexpr BitboardTable KING_ATTACKS = [] {
    BitboardTable table = {};
    for (auto sq = 0; sq < N_SQUARES; ++sq) {
        auto file  = fileOf(Square(sq));
        auto rank  = rankOf(Square(sq));
        table[sq]  = 0;
        table[sq] |= addSquare(file - 1, rank - 1);
        table[sq] |= addSquare(file - 1, rank + 1);
        table[sq] |= addSquare(file + 1, rank - 1);
        table[sq] |= addSquare(file + 1, rank + 1);
        table[sq] |= addSquare(file, rank - 1);
        table[sq] |= addSquare(file, rank + 1);
        table[sq] |= addSquare(file - 1, rank);
        table[sq] |= addSquare(file + 1, rank);
    }
    return table;
}();

constexpr BitboardMatrix BITLINE = [] {
    BitboardMatrix table = {};

    auto populateDiagonal = [](int rank, int file, int rankIncr, int fileIncr) -> U64 {
        U64 mask = 0;
        while (0 <= rank && rank < 8 && 0 <= file && file < 8) {
            mask |= (1ULL << (rank * 8 + file));
            rank += rankIncr;
            file += fileIncr;
        }
        return mask;
    };

    for (auto sq1 = 0; sq1 < N_SQUARES; ++sq1) {
        for (int sq2 = 0; sq2 < N_SQUARES; ++sq2) {
            int rank1 = sq1 / 8, file1 = sq1 % 8;
            int rank2 = sq2 / 8, file2 = sq2 % 8;

            table[sq1][sq2] = 0;
            if (rank1 == rank2) {
                table[sq1][sq2] = RANK_MASK[rank1];
            } else if (file1 == file2) {
                table[sq1][sq2] = FILE_MASK[file1];
            } else if ((rank1 - rank2) == (file1 - file2)) {
                // ↙↗ diagonal
                table[sq1][sq2] =
                    populateDiagonal(rank1, file1, 1, 1) | populateDiagonal(rank1, file1, -1, -1);
            } else if ((rank1 + file1) == (rank2 + file2)) {
                // ↖↘ diagonal
                table[sq1][sq2] =
                    populateDiagonal(rank1, file1, 1, -1) | populateDiagonal(rank1, file1, -1, 1);
            }
        }
    }
    return table;
}();

constexpr BitboardMatrix BITBTWN = [] {
    BitboardMatrix table = {};

    auto setBitsBetween = [](auto& table, int sq1, int sq2, int delta) {
        int min_sq = std::min(sq1, sq2);
        int max_sq = std::max(sq1, sq2);
        for (int s = (min_sq + delta); s < max_sq; s += delta) {
            table[sq1][sq2] |= (1ULL << s);
        }
    };

    for (auto sq1 = 0; sq1 < N_SQUARES; ++sq1) {
        for (int sq2 = 0; sq2 < N_SQUARES; ++sq2) {
            int rank1 = sq1 / 8, file1 = sq1 % 8;
            int rank2 = sq2 / 8, file2 = sq2 % 8;

            table[sq1][sq2] = 0;
            if (rank1 == rank2) {
                // Same rank
                setBitsBetween(table, sq1, sq2, 1);
            } else if (file1 == file2) {
                // Same file
                setBitsBetween(table, sq1, sq2, 8);
            } else if ((rank1 + file1) == (rank2 + file2)) {
                // ↙↗ diagonal
                setBitsBetween(table, sq1, sq2, 7);
            } else if ((rank1 - rank2) == (file1 - file2)) {
                // ↖↘ diagonal
                setBitsBetween(table, sq1, sq2, 9);
            }
        }
    }
    return table;
}();

constexpr U64 rank(Rank r) { return RANK_MASK[r]; }
constexpr U64 file(File f) { return FILE_MASK[f]; }
constexpr U64 set(const Square sq) { return BB::BITSET[sq]; }
constexpr U64 clear(const Square sq) { return BB::BITCLEAR[sq]; }
constexpr U64 inlineBB(const Square sq1, const Square sq2) { return BITLINE[sq1][sq2]; }
constexpr U64 betweenBB(const Square sq1, const Square sq2) { return BITBTWN[sq1][sq2]; }
constexpr U64 hasMoreThanOne(U64 bb) { return bb & (bb - 1); }
inline int count(U64 bb) { return __builtin_popcountll(bb); }
inline Square lsb(U64 bb) { return static_cast<Square>(__builtin_ctzll(bb)); }
inline Square msb(U64 bb) { return static_cast<Square>(63 - __builtin_clzll(bb)); }
inline Square lsbPop(U64& bb) {
    Square sq = static_cast<Square>(__builtin_ctzll(bb));
    bb &= ~(1ULL << sq);
    return sq;
}
inline Square msbPop(U64& bb) {
    Square sq = static_cast<Square>(__builtin_ctzll(bb));
    bb &= ~(1ULL << sq);
    return sq;
}


template <Color c>
inline Square advancedSq(U64 bb) {
    return (c == WHITE) ? msb(bb) : lsb(bb);
}

inline U64 fillNorth(U64 bb) {
    bb |= (bb << 8);
    bb |= (bb << 16);
    bb |= (bb << 32);
    return bb;
}

inline U64 fillSouth(U64 bb) {
    bb |= (bb >> 8);
    bb |= (bb >> 16);
    bb |= (bb >> 32);
    return bb;
}

inline U64 fillFiles(U64 bb) { return fillNorth(bb) | fillSouth(bb); }
inline U64 shiftSouth(U64 bb) { return bb >> 8; }
inline U64 shiftNorth(U64 bb) { return bb << 8; }
inline U64 shiftEast(U64 bb) { return (bb << 1) & ~BB::file(FILE1); }
inline U64 shiftWest(U64 bb) { return (bb >> 1) & ~BB::file(FILE8); }
inline U64 spanNorth(U64 bb) { return shiftNorth(fillNorth(bb)); }
inline U64 spanSouth(U64 bb) { return shiftSouth(fillSouth(bb)); }

template <Color c>
inline U64 spanFront(U64 bb) {
    return (c == WHITE) ? spanNorth(bb) : spanSouth(bb);
}

template <Color c>
inline U64 spanBack(U64 bb) {
    return (c == WHITE) ? spanSouth(bb) : spanNorth(bb);
}

template <Color c>
inline U64 pawnAttackSpan(U64 bb) {
    U64 spanFrontBB = spanFront<c>(bb);
    return shiftWest(spanFrontBB) | shiftEast(spanFrontBB);
}

template <Color c>
inline U64 pawnFullSpan(U64 bb) {
    // Returns the full pawn span, including both the forward moves and attacks
    // to the left and right diagonals, based on the color.
    U64 spanFrontBB = spanFront<c>(bb);
    return shiftWest(spanFrontBB) | shiftEast(spanFrontBB) | spanFrontBB;
}

template <PawnMove p, Color c>
inline U64 pawnMoves(U64 pawns) {
    if constexpr (p == LEFT) {
        constexpr U64 filemask  = file(c == WHITE ? FILE1 : FILE8);
        pawns                  &= ~filemask;
    } else if constexpr (p == RIGHT) {
        constexpr U64 filemask  = file(c == WHITE ? FILE8 : FILE1);
        pawns                  &= ~filemask;
    }

    constexpr int shift = static_cast<int>(p);
    return (c == WHITE) ? (pawns << shift) : (pawns >> shift);
}

template <PawnMove p>
inline U64 pawnMoves(U64 pawns, Color c) {
    return (c == WHITE) ? pawnMoves<p, WHITE>(pawns) : pawnMoves<p, BLACK>(pawns);
};

template <Color c>
inline U64 pawnAttacks(U64 pawns) {
    return pawnMoves<LEFT, c>(pawns) | pawnMoves<RIGHT, c>(pawns);
}

inline U64 pawnAttacks(U64 pawns, Color c) {
    return pawnMoves<LEFT>(pawns, c) | pawnMoves<RIGHT>(pawns, c);
}

template <PieceType p>
inline U64 pieceMoves(Square sq, U64 occupancy) {
    if constexpr (p == KNIGHT)
        return KNIGHT_ATTACKS[sq];
    else if constexpr (p == BISHOP)
        return Magics::getBishopAttacks(sq, occupancy);
    else if constexpr (p == ROOK)
        return Magics::getRookAttacks(sq, occupancy);
    else if constexpr (p == QUEEN)
        return Magics::getQueenAttacks(sq, occupancy);
    else if constexpr (p == KING)
        return KING_ATTACKS[sq];
    else
        return 0;
}

inline U64 pieceMoves(Square sq, PieceType p, U64 occupancy) {
    switch (p) {
        case KNIGHT: return pieceMoves<KNIGHT>(sq, occupancy);
        case BISHOP: return pieceMoves<BISHOP>(sq, occupancy);
        case ROOK: return pieceMoves<ROOK>(sq, occupancy);
        case QUEEN: return pieceMoves<QUEEN>(sq, occupancy);
        case KING: return pieceMoves<KING>(sq, occupancy);
        default: return 0;
    }
}

template <PieceType p>
inline U64 pieceMoves(Square sq) {
    return pieceMoves<p>(sq, 0);
}

struct Printer {
    U64 value;

    friend std::ostream& operator<<(std::ostream& os, const Printer& bb) {
        char output[64];

        // Generate visual representation of the bitboard.
        for (unsigned i = 0; i < 64; i++) {
            output[i] = (bb.value & BB::BITSET[i]) ? '1' : '.';
        }

        os << "\n  abcdefgh\n";

        // Print each rank of the chessboard.
        for (int rank = 7; rank >= 0; rank--) {
            os << "  ";

            for (int file = 0; file < 8; file++) {
                os << output[makeSquare(File(file), Rank(rank))];
            }

            os << " " << (rank + 1) << "\n";
        }

        os << std::endl;
        return os;
    }
};

}  // namespace BB
