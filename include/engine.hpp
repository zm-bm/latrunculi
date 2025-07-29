#pragma once

#include <string>
#include <unordered_map>

#include "board.hpp"
#include "thread_pool.hpp"
#include "uci.hpp"

class Engine {
public:
    Engine() = delete;
    Engine(std::ostream& out, std::ostream& err, std::istream& in);
    void loop();

    using Command    = std::function<bool(std::istringstream&)>;
    using CommandMap = std::unordered_map<std::string, Command>;

private:
    bool execute(const std::string&) noexcept;

    // UCI commands
    bool uci(std::istringstream& iss);
    bool set_debug(std::istringstream& iss);
    bool is_ready(std::istringstream& iss);
    bool set_option(std::istringstream& iss);
    bool new_game(std::istringstream& iss);
    bool position(std::istringstream& iss);
    bool go(std::istringstream& iss);
    bool stop(std::istringstream& iss);
    bool quit(std::istringstream& iss);
    bool ponder_hit(std::istringstream& iss);

    // Non-UCI commands
    bool help(std::istringstream& iss);
    bool display_board(std::istringstream& iss);
    bool evaluate(std::istringstream& iss);
    bool perft(std::istringstream& iss);
    bool move(std::istringstream& iss);
    bool moves(std::istringstream& iss);

    std::pair<std::string, std::string> parse_position(std::istringstream& iss);
    std::pair<std::string, std::string> parse_option(std::istringstream& iss);

    Move get_move(const std::string& token);

    std::ostream& out;
    std::ostream& err;
    std::istream& in;
    uci::Protocol protocol;
    uci::Config   config;
    Board         board;
    ThreadPool    thread_pool;
    CommandMap    command_map;

    friend class EngineTest;
    friend class Benchmark;
};
