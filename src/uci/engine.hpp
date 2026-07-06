#pragma once

#include <cstddef>
#include <deque>
#include <iosfwd>
#include <string>

#include "board/board.hpp"
#include "uci/threading.hpp"
#include "uci/uci_input.hpp"
#include "uci/uci_options.hpp"
#include "uci/uci_writer.hpp"

class Engine {
public:
    Engine() = delete;
    Engine(std::ostream& out, std::ostream& err, std::istream& source);
    void loop();

private:
    bool execute(const std::string&) noexcept;
    bool execute(const uci::Command& command) noexcept;
    bool dispatch(const uci::Command& command);

    // Command handlers
    bool handle(const uci::EmptyCommand&);
    bool handle(const uci::UciCommand&);
    bool handle(const uci::DebugCommand& command);
    bool handle(const uci::IsReadyCommand&);
    bool handle(const uci::SetOptionCommand& command);
    bool handle(const uci::NewGameCommand&);
    bool handle(const uci::PositionCommand& command);
    bool handle(const uci::GoCommand& command);
    bool handle(const uci::StopCommand&);
    bool handle(const uci::PonderHitCommand&);
    bool handle(const uci::RegisterCommand&);
    bool handle(const uci::QuitCommand&);
    bool handle(const uci::ExitCommand&);
    bool handle(const uci::ConsoleCommand& command);
    bool handle(const uci::UnknownCommand&);

    // Console extension handlers
    bool help();
    bool display_board();
    bool evaluate();
    bool perft(const std::string& arguments);
    bool move(const std::string& arguments);
    bool moves();

    // Board position helpers
    Move           find_legal_move(const std::string& token);
    PositionState& next_position_state();
    void           reset_board(const std::string& fen);
    void           make_board_move(Move move);
    void           unmake_board_move();

    // Option and search helpers
    void apply_option_effect(uci::OptionId option);

    uci::Reader               reader;
    uci::Writer               writer;
    uci::Options              options;
    std::deque<PositionState> position_states = {PositionState()};
    size_t                    position_ply    = 0;
    Board                     board;
    ThreadPool                thread_pool;

    friend class EngineTest;
    friend class Benchmark;
};
