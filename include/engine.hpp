#pragma once

#include <string>

#include "board.hpp"
#include "constants.hpp"
#include "search_stats.hpp"
#include "thread_pool.hpp"
#include "uci_options.hpp"
#include "uci_output.hpp"

class Engine {
   public:
    Engine() = delete;

    Engine(std::ostream& out, std::istream& in)
        : in(in), out(out), uciOutput(out), threadpool(uciOptions.threads, uciOutput) {}

    void loop();

    friend class EngineTest;

   private:
    std::istream& in;
    std::ostream& out;
    UCIOutput uciOutput;
    UCIOptions uciOptions;
    ThreadPool threadpool;
    Board board = Board(STARTFEN);

    bool execute(const std::string&);

    void position(std::istringstream& iss);
    void go(std::istringstream& iss);
    void setdebug(std::istringstream& iss);
    void perft(std::istringstream& iss);
    void move(std::istringstream& iss);
    void moves();
};
