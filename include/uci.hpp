#pragma once

#include <ostream>
#include <string>

#include "base.hpp"
#include "search_stats.hpp"
#include "types.hpp"

struct UCIOptions {
    bool debug = DEFAULT_DEBUG;
};

struct UCIBestLine {
    UCIBestLine(int score, int depth, U64 nodes, Milliseconds time, std::string& pv)
        : score(score), depth(depth), nodes(nodes), time(time), pv(pv) {}

    std::string formatScore() const;
    friend std::ostream& operator<<(std::ostream& os, const UCIBestLine& info);

    int score;
    int depth;
    U64 nodes;
    Milliseconds time;
    std::string pv;
};

class UCIProtocolHandler {
   public:
    explicit UCIProtocolHandler(std::ostream& out, std::ostream& err) : out(out), err(err) {}

    void help() const;
    void identify() const;
    void ready() const;
    void bestmove(std::string) const;
    void info(const UCIBestLine& info) const;
    void info(const std::string& str) const;

    template <typename T>
    void logOutput(T&& obj) const {
        err << std::forward<T>(obj) << std::endl;
    }

   private:
    std::ostream& out;
    std::ostream& err;
};
