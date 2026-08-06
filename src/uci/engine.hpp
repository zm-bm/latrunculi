#pragma once

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "board/board.hpp"
#include "search/thread_pool.hpp"
#include "uci/command.hpp"
#include "uci/options.hpp"
#include "uci/writer.hpp"

class EngineTest;

namespace uci {

class Engine {
public:
    Engine() = delete;
    Engine(std::ostream& output, std::ostream& diagnostics, std::istream& input);
    void loop();

private:
    bool execute(const std::string&) noexcept;
    bool execute(const Command& command) noexcept;
    bool dispatch(const Command& command);

    // Command handlers
    bool handle(const EmptyCommand&);
    bool handle(const UciCommand&);
    bool handle(const DebugCommand& command);
    bool handle(const IsReadyCommand&);
    bool handle(const SetOptionCommand& command);
    bool handle(const NewGameCommand&);
    bool handle(const PositionCommand& command);
    bool handle(const GoCommand& command);
    bool handle(const StopCommand&);
    bool handle(const PonderHitCommand&);
    bool handle(const RegisterCommand&);
    bool handle(const QuitCommand&);
    bool handle(const ExitCommand&);
    bool handle(const ConsoleCommand& command);
    bool handle(const UnknownCommand&);

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
    void apply_option_effect(OptionId option, const Options& candidate);
    void require_idle(std::string_view action) const;

    std::istream&      input;
    Writer             writer;
    Options            options;
    bool               debug_mode{false};
    Board              board;
    search::ThreadPool thread_pool;

    friend class ::EngineTest;
};

} // namespace uci
