#pragma once

#include <string>

#include "board.hpp"
#include "constants.hpp"
#include "search_stats.hpp"
#include "thread_pool.hpp"
#include "uci.hpp"

class Engine {
   public:
    Engine() = delete;

    Engine(std::ostream& out, std::istream& in)
        : in(in),
          out(out),
          uciHandler(out),
          board(STARTFEN),
          threadpool(DEFAULT_THREADS, uciHandler) {}

    void loop();

    friend class EngineTest;

   private:
    std::istream& in;
    std::ostream& out;
    UCIProtocolHandler uciHandler;
    UCIOptions uciOptions;
    Board board;
    ThreadPool threadpool;

    // Execute command
    bool execute(const std::string&);

    // UCI commands
    void uci(std::istringstream& iss);
    void setdebug(std::istringstream& iss);
    void isready(std::istringstream& iss);
    void setoption(std::istringstream& iss);
    void newgame(std::istringstream& iss);
    void position(std::istringstream& iss);
    void go(std::istringstream& iss);
    void stop(std::istringstream& iss);
    void ponderhit(std::istringstream& iss);

    // Non-UCI commands
    void help(std::istringstream& iss);
    void display(std::istringstream& iss);
    void evaluate(std::istringstream& iss);
    void perft(std::istringstream& iss);
    void move(std::istringstream& iss);
    void moves(std::istringstream& iss);

    // Helper functions
    std::string parseFen(std::istringstream& iss);
    void loadMoves(std::istringstream& iss);
    bool tryMove(Board& board, const std::string& token);
    void parseSetoptionInt(
        std::istringstream& iss, const std::string& opt, int min, int max, auto&& handler);

    friend class BenchmarkTest;
};
