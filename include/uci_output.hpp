#pragma once

#include <ostream>
#include <string>

#include "search_stats.hpp"
#include "types.hpp"

class UCIOutput {
   public:
    explicit UCIOutput(std::ostream& out) : out(out) {}

    // UCI protocol commands
    void identify() const;
    void ready() const;
    void bestmove(std::string) const;
    void info(int, int, U64, Milliseconds, std::string, bool = false);
    void infoString(const std::string& str) const;

    // Non UCI protocol commands
    void stats(SearchStats<> stats) const;
    void help() const;
    void unknownCommand(const std::string& command) const;

   private:
    std::ostream& out;
    int lastScore{0};
    std::string lastPV;

    std::string formatScore(int score);
};
