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

template <bool>
int negamax(Thread&, int, int, int);

template <bool>
U64 perft(int, Chess&, std::ostream& = std::cout);

}
