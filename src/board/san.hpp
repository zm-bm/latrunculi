#pragma once

#include <string>

#include "core/move.hpp"

class Board;

// Precondition: move is legal in board's current position.
[[nodiscard]] std::string to_san(const Board& board, Move move);
