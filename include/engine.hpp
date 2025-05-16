#pragma once

#include <string>

#include "board.hpp"
#include "constants.hpp"
#include "statistics.hpp"
#include "thread.hpp"

// forward declare
struct Move;

/**
 * @brief UCI chess engine
 * http://wbec-ridderkerk.nl/html/UCIProtocol.html
 */
class Engine {
   public:
    Engine(std::ostream& out, std::istream& in) : out(out), in(in) {}

    void loop();
    bool execute(const std::string&);

    // UCI output by search thread
    void bestmove(Move);
    void info(int, int, Milliseconds, PrincipalVariation&);
    void searchStats();

   private:
    // Engine state
    Board board = Board(STARTFEN);

    // Search thread pool
    ThreadPool threadpool = ThreadPool(DEFAULT_THREADS, this);

    // UCI options
    bool debug = DEFAULT_DEBUG;

    // I/O streams
    std::ostream& out;
    std::istream& in;

    // UCI commands
    void position(std::istringstream& iss);
    void go(std::istringstream& iss);
    void uci();

    // Helper commands
    void setdebug(std::istringstream& iss);
    void perft(std::istringstream& iss);
    void move(std::istringstream& iss);
    void moves();
};
