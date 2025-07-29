#pragma once

#include <array>
#include <ostream>

#include "defs.hpp"
#include "magic.hpp"
#include "util.hpp"

namespace bb {

constexpr uint64_t set(const Square sq) {
    return 0x1ULL << sq;
}

constexpr uint64_t clear(const Square sq) {
    return ~(0x1ULL << sq);
}

template <typename... Squares>
constexpr uint64_t set(const Squares... sqs) {
    return (set(sqs) | ...);
}

constexpr uint64_t file(const File file) {
    return 0x0101010101010101ULL << file;
}

constexpr uint64_t rank(const Rank rank) {
    return 0xFFULL << (rank * 8);
}

constexpr int count(const uint64_t bitboard) {
    return __builtin_popcountll(bitboard);
}

constexpr uint64_t is_many(const uint64_t bitboard) {
    return bitboard & (bitboard - 1);
}

constexpr Square lsb(const uint64_t bitboard) {
    return Square(__builtin_ctzll(bitboard));
}

constexpr Square msb(const uint64_t bitboard) {
    return Square(63 - __builtin_clzll(bitboard));
}

template <Color C>
constexpr Square select(const uint64_t bitboard) {
    return (C == WHITE) ? msb(bitboard) : lsb(bitboard);
}

constexpr Square lsb_pop(uint64_t& bitboard) {
    auto sq   = lsb(bitboard);
    bitboard &= clear(sq);
    return sq;
}

constexpr Square msb_pop(uint64_t& bitboard) {
    auto sq   = msb(bitboard);
    bitboard &= clear(sq);
    return sq;
}

template <Color C>
constexpr Square pop(uint64_t& bitboard) {
    return (C == WHITE) ? msb_pop(bitboard) : lsb_pop(bitboard);
}

template <Color C, typename Func>
inline void scan(uint64_t& bitboard, Func action) {
    while (bitboard) {
        auto sq = bb::pop<C>(bitboard);
        action(sq);
    }
}

constexpr uint64_t fill_north(uint64_t bitboard) {
    bitboard |= (bitboard << 8);
    bitboard |= (bitboard << 16);
    bitboard |= (bitboard << 32);
    return bitboard;
}

constexpr uint64_t fill_south(uint64_t bitboard) {
    bitboard |= (bitboard >> 8);
    bitboard |= (bitboard >> 16);
    bitboard |= (bitboard >> 32);
    return bitboard;
}

constexpr uint64_t fill(uint64_t bitboard) {
    return fill_north(bitboard) | fill_south(bitboard);
}

constexpr uint64_t shift_south(uint64_t bitboard) {
    return bitboard >> 8;
}

constexpr uint64_t shift_north(uint64_t bitboard) {
    return bitboard << 8;
}

constexpr uint64_t shift_east(uint64_t bitboard) {
    return (bitboard << 1) & ~bb::file(FILE1);
}

constexpr uint64_t shift_west(uint64_t bitboard) {
    return (bitboard >> 1) & ~bb::file(FILE8);
}

constexpr uint64_t span_north(uint64_t bitboard) {
    return shift_north(fill_north(bitboard));
}

constexpr uint64_t span_south(uint64_t bitboard) {
    return shift_south(fill_south(bitboard));
}

template <Color C>
constexpr uint64_t span_front(uint64_t bitboard) {
    return (C == WHITE) ? span_north(bitboard) : span_south(bitboard);
}

template <Color C>
constexpr uint64_t span_back(uint64_t bitboard) {
    return (C == WHITE) ? span_south(bitboard) : span_north(bitboard);
}

template <Color C>
constexpr uint64_t attack_span(uint64_t bitboard) {
    uint64_t frontspan = span_front<C>(bitboard);
    return shift_west(frontspan) | shift_east(frontspan);
}

template <Color C>
constexpr uint64_t full_span(uint64_t bitboard) {
    uint64_t frontspan = span_front<C>(bitboard);
    return shift_west(frontspan) | shift_east(frontspan) | frontspan;
}

template <PawnMove M, Color C>
constexpr uint64_t pawn_moves(uint64_t pawns) {
    if constexpr (M == PAWN_LEFT || M == PAWN_RIGHT) {
        constexpr File edge  = ((M == PAWN_LEFT) ^ (C == BLACK)) ? FILE1 : FILE8;
        pawns               &= ~file(edge);
    }

    return (C == WHITE) ? pawns << M : pawns >> M;
}

template <PawnMove M>
constexpr uint64_t pawn_moves(uint64_t pawns, Color c) {
    return (c == WHITE) ? pawn_moves<M, WHITE>(pawns) : pawn_moves<M, BLACK>(pawns);
};

template <Color C>
constexpr uint64_t pawn_attacks(uint64_t pawns) {
    return pawn_moves<PAWN_LEFT, C>(pawns) | pawn_moves<PAWN_RIGHT, C>(pawns);
}

constexpr uint64_t pawn_attacks(uint64_t pawns, Color c) {
    return pawn_moves<PAWN_LEFT>(pawns, c) | pawn_moves<PAWN_RIGHT>(pawns, c);
}

namespace {

template <typename T = uint64_t>
using LookupTable = std::array<T, N_SQUARES>;

template <typename T = uint64_t>
using LookupMatrix = std::array<LookupTable<T>, N_SQUARES>;

template <typename T = uint64_t>
constexpr LookupTable<T> lookup_table(auto func) {
    LookupTable<T> table = {};
    for (Square sq = A1; sq < N_SQUARES; ++sq)
        table[sq] = func(sq);
    return table;
}

template <typename T = uint64_t>
constexpr LookupMatrix<T> lookup_matrix(auto func) {
    LookupMatrix<T> table = {};
    for (Square sq1 = A1; sq1 < N_SQUARES; ++sq1) {
        for (Square sq2 = A1; sq2 < N_SQUARES; ++sq2)
            table[sq1][sq2] = func(sq1, sq2);
    }
    return table;
}

template <size_t N>
constexpr auto calculate_moves(const int (&moves)[N][2]) {
    return [moves](auto sq) {
        Rank r = rank_of(sq);
        File f = file_of(sq);

        uint64_t mask = 0;
        for (auto& move : moves) {
            Rank nr = r + move[0];
            File nf = f + move[1];
            if (0 <= nf && nf < 8 && 0 <= nr && nr < 8)
                mask |= set(make_square(nf, nr));
        }
        return mask;
    };
}

constexpr auto knight_moves = lookup_table(calculate_moves({
    {+2, -1},
    {+1, -2},
    {+1, +2},
    {+2, +1},
    {-2, -1},
    {-1, -2},
    {-1, +2},
    {-2, +1},
}));

constexpr auto king_moves = lookup_table(
    calculate_moves({{+1, -1}, {+1, 0}, {+1, +1}, {0, -1}, {0, +1}, {-1, -1}, {-1, 0}, {-1, +1}}));

constexpr auto distance_table = lookup_matrix<uint8_t>([](auto sq1, auto sq2) {
    auto file_diff = file_of(sq1) - file_of(sq2);
    auto rank_diff = rank_of(sq1) - rank_of(sq2);

    uint8_t f = (file_diff < 0) ? -file_diff : file_diff;
    uint8_t r = (rank_diff < 0) ? -rank_diff : rank_diff;

    return std::max(f, r);
});

constexpr auto collinear_helper(File f, Rank r, int f_delta, int r_delta) {
    uint64_t mask = 0;
    while (0 <= r && r < 8 && 0 <= f && f < 8) {
        mask |= set(make_square(f, r));
        f     = f + f_delta;
        r     = r + r_delta;
    }
    return mask;
};

constexpr auto collinear_table = lookup_matrix([](auto sq1, auto sq2) -> uint64_t {
    Rank r1 = rank_of(sq1), r2 = rank_of(sq2);
    File f1 = file_of(sq1), f2 = file_of(sq2);

    if (r1 == r2) {
        return rank(r1);
    } else if (f1 == f2) {
        return file(f1);
    } else if (int(r1 - r2) == int(f1 - f2)) {
        // A1-H8 diagonal
        return collinear_helper(f1, r1, 1, 1) | collinear_helper(f1, r1, -1, -1);
    } else if (int(r1) + int(f1) == int(r2) + int(f2)) {
        // H1-A8 diagonal
        return collinear_helper(f1, r1, -1, 1) | collinear_helper(f1, r1, 1, -1);
    }
    return 0;
});

constexpr auto between_helper(auto sq1, auto sq2, int delta) {
    int min_sq = std::min(sq1, sq2);
    int max_sq = std::max(sq1, sq2);

    uint64_t mask = 0;
    for (Square sq = Square(min_sq + delta); sq < max_sq; sq = sq + delta)
        mask |= set(sq);
    return mask;
};

constexpr auto between_table = lookup_matrix([](auto sq1, auto sq2) -> uint64_t {
    Rank r1 = rank_of(sq1), r2 = rank_of(sq2);
    File f1 = file_of(sq1), f2 = file_of(sq2);

    if (r1 == r2) {
        return between_helper(sq1, sq2, EAST);
    } else if (f1 == f2) {
        return between_helper(sq1, sq2, NORTH);
    } else if (int(r1 - r2) == int(f1 - f2)) {
        return between_helper(sq1, sq2, NORTH_EAST);
    } else if (int(r1) + int(f1) == int(r2) + int(f2)) {
        return between_helper(sq1, sq2, NORTH_WEST);
    }
    return 0;
});

} // anonymous namespace

template <PieceType p>
constexpr uint64_t moves(Square sq, uint64_t occupancy = 0) {
    switch (p) {
    case KNIGHT: return knight_moves[sq];
    case BISHOP: return magic::bishop_moves(sq, occupancy);
    case ROOK:   return magic::rook_moves(sq, occupancy);
    case QUEEN:  return magic::queen_moves(sq, occupancy);
    case KING:   return king_moves[sq];
    default:     return 0;
    }
}

constexpr uint64_t moves(Square sq, PieceType p, uint64_t occupancy) {
    switch (p) {
    case KNIGHT: return knight_moves[sq];
    case BISHOP: return magic::bishop_moves(sq, occupancy);
    case ROOK:   return magic::rook_moves(sq, occupancy);
    case QUEEN:  return magic::queen_moves(sq, occupancy);
    case KING:   return king_moves[sq];
    default:     return 0;
    }
}

// distance between two squares
constexpr uint8_t distance(const Square sq1, const Square sq2) {
    return distance_table[sq1][sq2];
}

// all squares on the same rank/file/diagonal of two squares
constexpr uint64_t collinear(const Square sq1, const Square sq2) {
    return collinear_table[sq1][sq2];
}

// all squares inbetween two squares
constexpr uint64_t between(const Square sq1, const Square sq2) {
    return between_table[sq1][sq2];
}

// Debugging utility to visualize a bitboard
struct Debug {
    uint64_t bitboard;

    friend std::ostream& operator<<(std::ostream& os, const Debug& debug) {
        for (Rank rank = RANK8; rank >= RANK1; --rank) {
            os << " ";
            for (File file = FILE1; file <= FILE8; ++file) {
                uint64_t bitset = debug.bitboard & bb::set(make_square(file, rank));
                os << (bitset ? '1' : '.');
            }
            os << " " << rank << "\n";
        }

        os << " abcdefgh" << std::endl;
        return os;
    }
};

} // namespace bb
