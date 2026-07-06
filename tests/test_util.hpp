#pragma once

#include <cstddef>
#include <deque>
#include <stdexcept>
#include <string>

#include "board/board.hpp"
#include "core/defs.hpp"

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

struct TestPositionStateOwner {
protected:
    std::deque<PositionState> position_states = {PositionState()};
    size_t                    ply             = 0;

    PositionState& root_state() { return position_states.front(); }

    PositionState& next_position_state() {
        if (position_states.size() <= ply + 1)
            position_states.emplace_back();
        return position_states[ply + 1];
    }
};

class TestBoard : private TestPositionStateOwner, public Board {
public:
    explicit TestBoard(const std::string& fen = STARTFEN)
        : Board(TestPositionStateOwner::root_state(), fen) {}

    using Board::make;
    using Board::make_null;
    using Board::unmake;
    using Board::unmake_null;

    void make(Move move) {
        Board::make(move, next_position_state());
        ++ply;
    }

    void unmake() {
        if (ply == 0)
            throw std::runtime_error("no move to undo");

        Board::unmake(position_states[ply - 1]);
        --ply;
    }

    void make_null() {
        Board::make_null(next_position_state());
        ++ply;
    }

    void unmake_null() {
        if (ply == 0)
            throw std::runtime_error("no null move to undo");

        Board::unmake_null(position_states[ply - 1]);
        --ply;
    }

    void reset(const std::string& fen = STARTFEN) {
        load_fen(fen);

        const auto root_state = position_state();
        position_states.clear();
        position_states.push_back(root_state);
        ply = 0;
        bind_position_state(position_states.front());
    }
};
