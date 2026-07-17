#pragma once

#include <cstddef>
#include <deque>
#include <stdexcept>
#include <string>
#include <string_view>

#include "board/board.hpp"
#include "board/ply_state.hpp"

namespace board_test {

// This storage must be a base so its root state exists before Board is constructed.
class StateStack {
protected:
    std::deque<PlyState> states_{1};
    std::size_t          ply_ = 0;

    PlyState& root_state() { return states_.front(); }

    PlyState& next_state() {
        if (states_.size() <= ply_ + 1)
            states_.emplace_back();
        return states_[ply_ + 1];
    }
};

class Harness : private StateStack, public Board {
public:
    explicit Harness(std::string_view fen = Board::startfen)
        : Board(StateStack::root_state(), std::string(fen)) {}

    using Board::make;
    using Board::make_null;
    using Board::unmake;
    using Board::unmake_null;

    void make(Move move) {
        Board::make(move, next_state());
        ++ply_;
    }

    void unmake() {
        if (ply_ == 0)
            throw std::runtime_error("no move to undo");

        Board::unmake(states_[ply_ - 1]);
        --ply_;
    }

    void make_null() {
        Board::make_null(next_state());
        ++ply_;
    }

    void unmake_null() {
        if (ply_ == 0)
            throw std::runtime_error("no null move to undo");

        Board::unmake_null(states_[ply_ - 1]);
        --ply_;
    }

    void reset(std::string_view fen = Board::startfen) {
        load_fen(std::string(fen));

        const auto root_state = ply_state();
        states_.clear();
        states_.push_back(root_state);
        ply_ = 0;
        bind_ply_state(states_.front());
    }
};

} // namespace board_test
