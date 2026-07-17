#pragma once

#include <string>

#include "core/move.hpp"

class Board;

std::string to_san(const Board& board, Move move);
