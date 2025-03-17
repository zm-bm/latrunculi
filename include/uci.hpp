#pragma once

#include <string>

#include "board.hpp"
#include "search.hpp"
#include "thread.hpp"

// Universal Chess Interface (UCI)
// http://wbec-ridderkerk.nl/html/UCIProtocol.html
namespace UCI {

class Engine {
   public:
    Engine(std::istream&, std::ostream&);
    void loop();
    bool execute(const std::string&);

   private:
    Board board;
    ThreadPool threads;
    bool debug;

    std::istream& istream;
    std::ostream& ostream;

    void uci();
    void setdebug(std::istringstream& iss);
    void position(std::istringstream& iss);
    void perft(std::istringstream& iss);
    void go(std::istringstream& iss);
    void move(std::istringstream& iss);
    void moves();
};

}  // namespace UCI
