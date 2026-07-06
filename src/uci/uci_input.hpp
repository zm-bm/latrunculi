#pragma once

#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

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
struct GoLimits {
    std::optional<int> depth;
    std::optional<int> movetime;
    std::optional<int> nodes;
    std::optional<int> wtime;
    std::optional<int> btime;
    std::optional<int> winc;
    std::optional<int> binc;
    std::optional<int> movestogo;

    bool                     ponder{false};
    bool                     infinite{false};
    std::optional<int>       mate;
    std::vector<std::string> searchmoves;
    std::vector<std::string> unknown_tokens;
};
struct GoCommand {
    GoLimits limits;
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

Command parse_command(std::string_view line);

// UCI stdin reader; parsing stays in parse_command().
class Reader {
public:
    explicit Reader(std::istream& in) : in(in) {}

    std::optional<Command> read_command();

private:
    std::istream& in;
};

} // namespace uci
