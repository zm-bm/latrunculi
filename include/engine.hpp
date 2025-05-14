#pragma once

#include <string>

#include "board.hpp"
#include "constants.hpp"
#include "context.hpp"
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

    // UCI output
    void uci();
    void bestmove(Move);
    void info(int, int, PrincipalVariation&, double);
    void searchStats();

   private:
    Board board           = Board(STARTFEN);
    ThreadPool threadpool = ThreadPool(SEARCH_THREADS, this);
    Context context;

    std::ostream& out;
    std::istream& in;

    // UCI input
    void setdebug(std::istringstream& iss);
    void position(std::istringstream& iss);
    void perft(std::istringstream& iss);
    void go(std::istringstream& iss);
    void move(std::istringstream& iss);
    void moves();
};
