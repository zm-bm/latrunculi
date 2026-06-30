#pragma once

#include <cstdint>
#include <functional>
#include <ostream>
#include <string>

#include "defs.hpp"

class Board;
struct RootLine;

// UCI Protocol: https://www.wbec-ridderkerk.nl/html/UCIProtocol.html

namespace uci {

struct SpinOption {
    int  value;
    int  def_value;
    int  min_value;
    int  max_value;
    void set(std::string value_str);
};

struct CheckOption {
    bool value;
    bool def_value;
    void set(std::string val_str);
};

struct Config {
    SpinOption hash = {
        .value     = DEFAULT_HASH,
        .def_value = DEFAULT_HASH,
        .min_value = 1,
        .max_value = 2048,
    };

    SpinOption threads = {
        .value     = DEFAULT_THREADS,
        .def_value = DEFAULT_THREADS,
        .min_value = 1,
        .max_value = 64,
    };

    CheckOption debug = {
        .value     = false,
        .def_value = false,
    };

    std::function<void(int)> hash_callback;
    std::function<void(int)> thread_callback;

    void set_option(const std::string& name, const std::string& value);
};

struct SearchInfo {
    int          score;
    int          depth;
    uint64_t     nodes;
    Milliseconds time;
    std::string  pv;

    std::string score_text() const;
    std::string nps_text() const;
};

SearchInfo
make_search_info(const RootLine& line, const Board& root_board, uint64_t nodes, Milliseconds time);

class Protocol {
public:
    explicit Protocol(std::ostream& out, std::ostream& err) : out(out), err(err) {}

    void help() const;
    void identify(const uci::Config& config) const;
    void ready() const;
    void bestmove(std::string move) const;
    void info(const uci::SearchInfo& info) const;
    void info(const std::string& str) const;

    template <typename T>
    void diagnostic_output(T&& obj) const;

private:
    std::ostream& out;
    std::ostream& err;
};

} // namespace uci
