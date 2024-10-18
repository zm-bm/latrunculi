#ifndef LATRUNCULI_DEFS_H
#define LATRUNCULI_DEFS_H

#include <array>
#include <sstream>
#include <string>
#include <vector>

#include "types.hpp"

namespace G {

const std::string STARTFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                  POS2 = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                  POS3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                  POS4W = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
                  POS4B = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
                  POS5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";

const std::string FENS[6] = {STARTFEN, POS2, POS3, POS4W, POS4B, POS5};

constexpr Rank RANK_BY_COLOR[NCOLORS][NRANKS] = {
    {RANK8, RANK7, RANK6, RANK5, RANK4, RANK3, RANK2, RANK1},
    {RANK1, RANK2, RANK3, RANK4, RANK5, RANK6, RANK7, RANK8},
};

constexpr File FILE_BY_COLOR[NCOLORS][NFILES] = {
    {FILE8, FILE7, FILE6, FILE5, FILE4, FILE3, FILE2, FILE1},
    {FILE1, FILE2, FILE3, FILE4, FILE5, FILE6, FILE7, FILE8},
};

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
constexpr std::array<std::array<int, NSQUARES>, NSQUARES> DISTANCE = genDISTANCE();

inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(s);

    while (std::getline(ss, token, delim)) {
        tokens.push_back(token);
    }

    return tokens;
}

}

#endif