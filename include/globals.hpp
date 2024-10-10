#ifndef LATRUNCULI_GLOBAL_H
#define LATRUNCULI_GLOBAL_H

#include <array>
#include <sstream>
#include <string>
#include <vector>

#include "types.hpp"

namespace G {

const std::string STARTFEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    POS2 = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    POS3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    POS4W = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    POS4B = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
    POS5 =  "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
const std::string FENS[6] =
    { STARTFEN, POS2, POS3, POS4W, POS4B, POS5 };

constexpr U64 RANK_MASK[8] = {
    0x00000000000000FF, 0x000000000000FF00, 0x0000000000FF0000,
    0x00000000FF000000, 0x000000FF00000000, 0x0000FF0000000000,
    0x00FF000000000000, 0xFF00000000000000,
};

constexpr Rank RANK[2][8] = {
    {RANK8, RANK7, RANK6, RANK5, RANK4, RANK3, RANK2, RANK1},
    {RANK1, RANK2, RANK3, RANK4, RANK5, RANK6, RANK7, RANK8},
};

inline U64 rankmask(Rank r, Color c) { return RANK_MASK[RANK[c][r]]; }

constexpr U64 FILE_MASK[8] = {
    0x0101010101010101, 0x202020202020202,  0x404040404040404,
    0x808080808080808,  0x1010101010101010, 0x2020202020202020,
    0x4040404040404040, 0x8080808080808080,
};

constexpr File FILE[2][8] = {
    {FILE8, FILE7, FILE6, FILE5, FILE4, FILE3, FILE2, FILE1},
    {FILE1, FILE2, FILE3, FILE4, FILE5, FILE6, FILE7, FILE8},
};

inline U64 filemask(File f, Color c) { return FILE_MASK[FILE[c][f]]; }

const U64 WHITESQUARES = 0x55AA55AA55AA55AA;
const U64 BLACKSQUARES = 0xAA55AA55AA55AA55;
const U64 WHITEHOLES = 0x0000003CFFFF0000;
const U64 BLACKHOLES = 0x0000FFFF3C000000;

const U64 CastlePathOO[2] = {0x6000000000000000ull, 0x0000000000000060ull};
const U64 CastlePathOOO[2] = {0x0E00000000000000ull, 0x000000000000000Eull};
const U64 KingCastlePathOO[2] = {0x7000000000000000ull, 0x0000000000000070ull};
const U64 KingCastlePathOOO[2] = {0x1C00000000000000ull, 0x000000000000001Cull};

const Square KingOrigin[2] = {E8, E1};
const Square KingDestinationOO[2] = {G8, G1};
const Square KingDestinationOOO[2] = {C8, C1};
const Square RookOriginOO[2] = {H8, H1};
const Square RookOriginOOO[2] = {A8, A1};

const int MobilityScaling[2][6] = {
    { 0,6,2,0,0,0 },
    { 2,3,1,1,1,1 }
};

const int PassedPawnBonus[2]    = {  30,  200 };
const int DoublePawnPenalty[2]  = { -30, -100 };
const int TriplePawnPenalty[2]  = { -45, -100 };
const int IsoPawnPenalty[2]     = { -30,  -40 };
const int OpenFileBonus[2]      = {  20,   10 };
const int HalfOpenFileBonus[2]  = {  10,    0 };
const int BishopPairBonus[2]    = {  20,   60 };

const int TEMPO_BONUS                 = 25;
const int KNIGHT_PENALTY_PER_PAWN     = -2;
const int ROOK_BONUS_PER_PAWN         = 2;
const int CONNECTED_ROOK_BONUS        = 15;
const int ROOK_ON_SEVENTH_BONUS       = 20;
const int BACK_RANK_MINOR_PENALTY     = -6;
const int MINOR_OUTPOST_BONUS         = 10;
const int STRONG_KING_SHIELD_BONUS    = 10;
const int WEAK_KING_SHIELD_BONUS      = 5;

const int PieceValues[6][2] = {
    { -PAWNSCORE,   PAWNSCORE },
    { -KNIGHTSCORE, KNIGHTSCORE },
    { -BISHOPSCORE, BISHOPSCORE },
    { -ROOKSCORE,   ROOKSCORE },
    { -QUEENSCORE,  QUEENSCORE },
    { -KINGSCORE,   KINGSCORE }
};

const int KNIGHT_TROPISM[8] = {
    0, 5, 4, 2, 0, 0, -1, -3
};

const int BISHOP_TROPISM[8] = {
    0, 5, 4, 3, 2, 1, 0, 0
};

const int ROOK_TROPISM[8] = {
    0, 6, 5, 3, 2, 1, 0, 0
};

const int QUEEN_TROPISM[8] = {
    0, 12, 10, 6, 4, 2, 0, -2
};

const int QUEEN_EARLY_DEV_PENALTY[4] = {
    0, -2, -8, -24
};

const int PieceSqValues[6][2][64] =
{
    { // Pawn piece square values
        {
            0,  0,  0,  0,  0,  0,  0,  0,
            50, 50, 50, 50, 50, 50, 50, 50,
            10, 10, 20, 30, 30, 20, 10, 10,
            5,  5, 10, 25, 25, 10,  5,  5,
            0,  0,  0, 20, 20,  0,  0,  0,
            5, -5,-10,  0,  0,-10, -5,  5,
            5, 10, 10,-20,-20, 10, 10,  5,
            0,  0,  0,  0,  0,  0,  0,  0
        }, // Opening

        {
                0,  0,  0,  0,  0,  0,  0,  0,
            115,125,125,125,125,125,125,125,
            85, 95, 95,105,105, 95, 95, 85,
            75, 85, 90,100,100, 90, 85, 65,
            65, 80, 80, 95, 95, 80, 80, 65,
            55, 75, 75, 75, 75, 75, 75, 55,
            50, 70, 70, 70, 70, 70, 70, 50,
                0,  0,  0,  0,  0,  0,  0,  0
        } // End game
    },

    { // Knight piece square values 
        {
            -50,-40,-30,-30,-30,-30,-40,-50,
            -40,-20,  0,  0,  0,  0,-20,-40,
            -30,  0, 10, 15, 15, 10,  0,-30,
            -30,  5, 15, 20, 20, 15,  5,-30,
            -30,  0, 15, 20, 20, 15,  0,-30,
            -30,  5, 10, 15, 15, 10,  5,-30,
            -40,-20,  0,  5,  5,  0,-20,-40,
            -50,-40,-30,-30,-30,-30,-40,-50,
        }, // Opening

        {
            -50,-40,-30,-30,-30,-30,-40,-50,
            -40,-20,  0,  0,  0,  0,-20,-40,
            -30,  0, 10, 15, 15, 10,  0,-30,
            -30,  5, 15, 20, 20, 15,  5,-30,
            -30,  0, 15, 20, 20, 15,  0,-30,
            -30,  5, 10, 15, 15, 10,  5,-30,
            -40,-20,  0,  5,  5,  0,-20,-40,
            -50,-40,-30,-30,-30,-30,-40,-50,
        }  // End game
    },

    { // Bishop piece square values
        {
            -20,-10,-10,-10,-10,-10,-10,-20,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -10,  0,  5, 10, 10,  5,  0,-10,
            -10,  5,  5, 10, 10,  5,  5,-10,
            -10,  0, 10, 10, 10, 10,  0,-10,
            -10, 10, 10, 10, 10, 10, 10,-10,
            -10, 10,  0,  0,  0,  0, 10,-10,
            -20,-10,-10,-10,-10,-10,-10,-20
        }, // Opening

        {
            -20,-10,-10,-10,-10,-10,-10,-20,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -10,  0,  5, 10, 10,  5,  0,-10,
            -10,  5,  5, 10, 10,  5,  5,-10,
            -10,  0, 10, 10, 10, 10,  0,-10,
            -10, 10, 10, 10, 10, 10, 10,-10,
            -10,  5,  0,  0,  0,  0,  5,-10,
            -20,-10,-10,-10,-10,-10,-10,-20
        }  // End game
    },

    { // Rook piece square values
        {
                0,  0,  0,  0,  0,  0,  0,  0,
                5, 10, 10, 10, 10, 10, 10,  5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                0,  0,  0,  5,  5,  0,  0,  0
        }, // Opening

        {
                0,  0,  0,  0,  0,  0,  0,  0,
                5, 10, 10, 10, 10, 10, 10,  5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                0,  0,  0,  5,  5,  0,  0,  0
        }  // End game
    },

    { // Queen piece square values 
        {
            -20,-10,-10, -5, -5,-10,-10,-20,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -10,  0,  5,  5,  5,  5,  0,-10,
                -5,  0,  5,  5,  5,  5,  0, -5,
                0,  0,  5,  5,  5,  5,  0, -5,
            -10,  5,  5,  5,  5,  5,  0,-10,
            -10,  0,  5,  0,  0,  0,  0,-10,
            -20,-10,-10, -5, -5,-10,-10,-20
        }, // Opening

        {
            -20,-10,-10, -5, -5,-10,-10,-20,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -10,  0,  5,  5,  5,  5,  0,-10,
                -5,  0,  5,  5,  5,  5,  0, -5,
                0,  0,  5,  5,  5,  5,  0, -5,
            -10,  5,  5,  5,  5,  5,  0,-10,
            -10,  0,  5,  0,  0,  0,  0,-10,
            -20,-10,-10, -5, -5,-10,-10,-20
        }  // End game
    },

    { // King piece square values 
        {
                30,-40,-40,-50,-50,-40,-40,-30,
            -30,-40,-40,-50,-50,-40,-40,-30,
            -30,-40,-40,-50,-50,-40,-40,-30,
            -30,-40,-40,-50,-50,-40,-40,-30,
            -20,-30,-30,-40,-40,-30,-30,-20,
            -10,-20,-20,-20,-20,-20,-20,-10,
                20, 20,  0,  0,  0,  0, 20, 20,
                20, 30, 10,  0,  0, 10, 30, 20
        }, // Opening 

        {
            -50,-40,-30,-20,-20,-30,-40,-50,
            -30,-20,-10,  0,  0,-10,-20,-30,
            -30,-10, 20, 30, 30, 20,-10,-30,
            -30,-10, 30, 40, 40, 30,-10,-30,
            -30,-10, 30, 40, 40, 30,-10,-30,
            -30,-10, 20, 30, 30, 20,-10,-30,
            -30,-30,  0,  0,  0,  0,-30,-30,
            -50,-30,-30,-30,-30,-30,-30,-50
        }  // End game
    }
};

const Square ColorSq[2][64] = {
    {
        A1, B1, C1, D1, E1, F1, G1, H1,
        A2, B2, C2, D2, E2, F2, G2, H2,
        A3, B3, C3, D3, E3, F3, G3, H3,
        A4, B4, C4, D4, E4, F4, G4, H4,
        A5, B5, C5, D5, E5, F5, G5, H5,
        A6, B6, C6, D6, E6, F6, G6, H6,
        A7, B7, C7, D7, E7, F7, G7, H7,
        A8, B8, C8, D8, E8, F8, G8, H8,
    },

    {
        H8, G8, F8, E8, D8, C8, B8, A8,
        H7, G7, F7, E7, D7, C7, B7, A7,
        H6, G6, F6, E6, D6, C6, B6, A6,
        H5, G5, F5, E5, D5, C5, B5, A5,
        H4, G4, F4, E4, D4, C4, B4, A4,
        H3, G3, F3, E3, D3, C3, B3, A3,
        H2, G2, F2, E2, D2, C2, B2, A2,
        H1, G1, F1, E1, D1, C1, B1, A1,
    }
};

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

constexpr std::array<std::array<U64, NSQUARES>, NSQUARES> genBITS_BETWEEN() {
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

inline std::vector<std::string> split(const std::string& s, char delim) {
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