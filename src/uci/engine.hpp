#pragma once

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

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
    Move              find_legal_move(const Board& position, const std::string& token) const;
    std::vector<Move> resolve_searchmoves(const std::vector<std::string>& tokens) const;
    void              make_board_move(Move move);
    void              unmake_board_move();

    // Option and search helpers
    void apply_option_effect(uci::OptionId option, const uci::Options& candidate);
    void require_idle(std::string_view action) const;

    uci::Reader  reader;
    uci::Writer  writer;
    uci::Options options;
    bool         debug_mode{false};
    Board        board;
    ThreadPool   thread_pool;

    friend class EngineTest;
    friend class Benchmark;
};
