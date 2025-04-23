#pragma once

#include <string>

#include "board.hpp"
#include "constants.hpp"
#include "thread.hpp"

struct Move;

// Universal Chess Interface (UCI)
// http://wbec-ridderkerk.nl/html/UCIProtocol.html
namespace UCI {

class Engine {
   public:
    Engine(std::ostream& out, std::istream& in) : out(out), in(in) {}

    void loop();
    bool execute(const std::string&);

   private:
    Board board           = Board(STARTFEN);
    ThreadPool threads    = ThreadPool(SEARCH_THREADS, this);
    SearchOptions options = {};
    std::ostream& out;
    std::istream& in;

    // input
    void setdebug(std::istringstream& iss);
    void position(std::istringstream& iss);
    void perft(std::istringstream& iss);
    void go(std::istringstream& iss);
    void move(std::istringstream& iss);
    void moves();

    // output
    void uci();
    void bestmove(Move);
    void info(int, int, SearchStats&, PrincipalVariation&);
    void searchStats(const SearchStats&);

    friend class ::Thread;
};

void printInfo(std::ostream&, int, int, SearchStats&, PrincipalVariation&);
void printDebuggingInfo(std::ostream&, const SearchStats&);

}  // namespace UCI
