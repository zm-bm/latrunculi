#pragma once

#include <numeric>
#include <vector>

#include "bb.hpp"
#include "defs.hpp"

const auto STARTFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const auto POS2     = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
const auto POS3     = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
const auto POS4W    = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
const auto POS4B    = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1";
const auto POS5     = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
const auto POS6     = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";
const auto EMPTYFEN = "4k3/8/8/8/8/8/8/4K3 w - - 0 1";
const auto PAWN_E2  = "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1";
const auto PAWN_E4  = "4k3/8/8/8/4P3/8/8/4K3 w - - 0 1";
const auto E2E4     = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
const auto ENPASSANT_A3 = "4k3/8/8/8/Pp6/8/8/4K3 b - a3 0 1";

static uint64_t targets(const std::vector<Square>& squares) {
    auto add_sq = [](uint64_t acc, Square sq) { return acc | bb::set(sq); };
    return std::accumulate(squares.begin(), squares.end(), 0ULL, add_sq);
}
