#pragma once

#include <cstddef>
#include <deque>
#include <string>

#include "board.hpp"
#include "threading.hpp"
#include "uci.hpp"

class Engine {
public:
    Engine() = delete;
    Engine(std::ostream& out, std::ostream& err, std::istream& in);
    void loop();

private:
    bool execute(const std::string&) noexcept;
    bool dispatch(const uci::Command& command);

    // UCI commands
    bool uci(const uci::UciCommand&);
    bool set_debug(const uci::DebugCommand& command);
    bool is_ready(const uci::IsReadyCommand&);
    bool set_option(const uci::SetOptionCommand& command);
    bool new_game(const uci::NewGameCommand&);
    bool position(const uci::PositionCommand& command);
    bool go(const uci::GoCommand& command);
    bool stop(const uci::StopCommand&);
    bool quit(const uci::QuitCommand&);
    bool ponder_hit(const uci::PonderHitCommand&);
    bool register_command(const uci::RegisterCommand&);
    bool exit(const uci::ExitCommand& command);
    bool unknown(const uci::UnknownCommand& command);
    bool empty(const uci::EmptyCommand&);

    // Non-UCI commands
    bool console(const uci::ConsoleCommand& command);
    bool help();
    bool display_board();
    bool evaluate();
    bool perft(const std::string& arguments);
    bool move(const std::string& arguments);
    bool moves();

    Move           get_move(const std::string& token);
    PositionState& next_position_state();
    void           reset_board(const std::string& fen);
    void           make_board_move(Move move);
    void           unmake_board_move();

    std::ostream&             out;
    std::ostream&             err;
    std::istream&             in;
    uci::Protocol             protocol;
    uci::Config               config;
    std::deque<PositionState> position_states = {PositionState()};
    size_t                    position_ply    = 0;
    Board                     board;
    ThreadPool                thread_pool;

    friend class EngineTest;
    friend class Benchmark;
};
