#pragma once

#include <ostream>
#include <string>

#include "base.hpp"
#include "search_stats.hpp"
#include "types.hpp"

struct UCIInfo {
    UCIInfo(int score, int depth, U64 nodes, Milliseconds time, std::string& pv)
        : score(score), depth(depth), nodes(nodes), time(time), pv(pv) {}

    std::string formatScore() const;
    friend std::ostream& operator<<(std::ostream& os, const UCIInfo& info);

    int score;
    int depth;
    U64 nodes;
    Milliseconds time;
    std::string pv;
};

class UCIOutput {
   public:
    explicit UCIOutput(std::ostream& out) : out(out) {}

    // UCI protocol commands
    void identify() const;
    void ready() const;
    void bestmove(std::string) const;

    void info(const UCIInfo& info);
    void info(const std::string& str) const;

    // Non UCI protocol commands
    void stats(SearchStats<> stats) const;
    void help() const;

   private:
    std::ostream& out;
    int lastScore{0};
    std::string lastPV;
};
