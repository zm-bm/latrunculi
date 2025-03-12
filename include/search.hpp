#pragma once

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iterator>
#include <memory>
#include <vector>

#include "move.hpp"
#include "types.hpp"

class Thread;
class Chess;

namespace Search {

enum class NodeType { Root, PV, NonPV };

template <NodeType>
int search(Thread&, int, int, int);

inline int search(Thread& thread, int alpha, int beta, int depth) {
    return search<NodeType::Root>(thread, alpha, beta, depth);
}


template <NodeType>
U64 perft(int, Chess&, std::ostream& = std::cout);

inline U64 perft(int depth, Chess& chess, std::ostream& oss = std::cout) {
    return perft<NodeType::Root>(depth, chess, oss);
}

}
