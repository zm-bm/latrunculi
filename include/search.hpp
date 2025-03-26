#pragma once

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iterator>
#include <memory>
#include <vector>

#include "constants.hpp"
#include "move.hpp"
#include "types.hpp"

class Thread;
class Board;

namespace Search {

constexpr bool isMateScore(int score) { return std::abs(score) > MATE_IN_MAX_PLY; }

constexpr int mateDistance(int score) { return MATE_VALUE - std::abs(score); }

enum class NodeType { Root, PV, NonPV };

int quiescence(Thread& thread, int alpha, int beta);

template <NodeType>
int search(Thread&, int, int, int);

inline int search(Thread& thread, int alpha, int beta, int depth) {
    return search<NodeType::Root>(thread, alpha, beta, depth);
}

template <NodeType>
U64 perft(int, Board&, std::ostream& = std::cout);

inline U64 perft(int depth, Board& board, std::ostream& oss = std::cout) {
    return perft<NodeType::Root>(depth, board, oss);
}

}  // namespace Search
