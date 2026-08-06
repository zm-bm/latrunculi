#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "core/types.hpp"

namespace uci {

// Parsed commands accepted by the engine input loop.
struct EmptyCommand {};
struct UciCommand {};
struct DebugCommand {
    std::string value;
};
struct IsReadyCommand {};
struct SetOptionCommand {
    std::string name;
    std::string value;
    bool        has_value{false};
};
struct NewGameCommand {};
struct PositionCommand {
    enum class Source { Invalid, Startpos, Fen };

    Source                   source{Source::Invalid};
    std::string              fen;
    std::vector<std::string> moves;
};
struct GoParameters {
    std::optional<int>               depth;
    std::optional<Milliseconds::rep> movetime;
    std::optional<NodeCount>         nodes;
    std::optional<Milliseconds::rep> wtime;
    std::optional<Milliseconds::rep> btime;
    std::optional<Milliseconds::rep> winc;
    std::optional<Milliseconds::rep> binc;
    std::optional<int>               movestogo;

    bool                                    ponder{false};
    bool                                    infinite{false};
    std::optional<int>                      mate;
    std::optional<std::vector<std::string>> searchmoves;
    std::vector<std::string>                unknown_tokens;
};
struct GoCommand {
    GoParameters parameters;
};
struct StopCommand {};
struct PonderHitCommand {};
struct RegisterCommand {};
struct QuitCommand {};
struct ExitCommand {};

// Latrunculi debug-console extensions. These are not official UCI commands, but
// they are accepted by the same command loop for local engine inspection.
struct ConsoleCommand {
    enum class Name { Help, Board, Eval, Move, Moves, Perft };

    Name        name;
    std::string arguments;
};
struct UnknownCommand {
    std::string token;
};

using Command = std::variant<EmptyCommand,
                             UciCommand,
                             DebugCommand,
                             IsReadyCommand,
                             SetOptionCommand,
                             NewGameCommand,
                             PositionCommand,
                             GoCommand,
                             StopCommand,
                             PonderHitCommand,
                             RegisterCommand,
                             QuitCommand,
                             ExitCommand,
                             ConsoleCommand,
                             UnknownCommand>;

} // namespace uci
