#pragma once

#include <string>
#include <unordered_map>

#include "board.hpp"
#include "constants.hpp"
#include "search_stats.hpp"
#include "thread_pool.hpp"
#include "uci.hpp"

class Engine {
   public:
    Engine() = delete;
    Engine(std::ostream& out, std::ostream& err, std::istream& in);

    void loop();

    friend class EngineTest;

   private:
    std::ostream& out;
    std::ostream& err;
    std::istream& in;
    UCIProtocolHandler uciHandler;
    UCIConfig config;
    Board board;
    ThreadPool threadpool;
    std::unordered_map<std::string, CommandFunc> commandMap;

    // Execute command
    bool execute(const std::string&) noexcept;

    // UCI commands
    bool uci(std::istringstream& iss);
    bool setdebug(std::istringstream& iss);
    bool isready(std::istringstream& iss);
    bool setoption(std::istringstream& iss);
    bool newgame(std::istringstream& iss);
    bool position(std::istringstream& iss);
    bool go(std::istringstream& iss);
    bool stop(std::istringstream& iss);
    bool quit(std::istringstream& iss);
    bool ponderhit(std::istringstream& iss);

    // Non-UCI commands
    bool help(std::istringstream& iss);
    bool displayBoard(std::istringstream& iss);
    bool evaluate(std::istringstream& iss);
    bool perft(std::istringstream& iss);
    bool move(std::istringstream& iss);
    bool moves(std::istringstream& iss);

    // Helpers
    std::pair<std::string, std::string> parsePosition(std::istringstream& iss);
    std::pair<std::string, std::string> parseSetOption(std::istringstream& iss);
    Move getMove(const std::string& token);

    friend class BenchmarkTest;
};
