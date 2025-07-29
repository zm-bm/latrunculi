#pragma once

#include <format>
#include <functional>
#include <ostream>
#include <print>
#include <string>

#include "defs.hpp"

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

    std::function<void(const int&)> hash_callback;
    std::function<void(const int&)> thread_callback;

    void set_option(const std::string& name, const std::string& value);
};

struct PV {
    int          score;
    int          depth;
    uint64_t     nodes;
    Milliseconds time;
    std::string  moves;

    std::string get_score() const;
    std::string get_nps() const;
};

class Protocol {
public:
    explicit Protocol(std::ostream& out, std::ostream& err) : out(out), err(err) {}

    void help() const;
    void identify(const uci::Config& config) const;
    void ready() const;
    void bestmove(std::string move) const;
    void info(const uci::PV& pv) const;
    void info(const std::string& str) const;

    template <typename T>
    void diagnostic_output(T&& obj) const;

private:
    std::ostream& out;
    std::ostream& err;
};

}; // namespace uci

template <>
struct std::formatter<uci::SpinOption> : std::formatter<std::string_view> {
    auto format(const uci::SpinOption& opt, std::format_context& ctx) const {
        return std::format_to(ctx.out(),
                              "type spin default {} min {} max {}",
                              opt.def_value,
                              opt.min_value,
                              opt.max_value);
    }
};

template <>
struct std::formatter<uci::PV> : std::formatter<std::string_view> {
    auto format(const uci::PV& pv, std::format_context& ctx) const {
        return std::format_to(ctx.out(),
                              "depth {} score {} nodes {} time {} nps {} pv {}",
                              pv.depth,
                              pv.get_score(),
                              pv.nodes,
                              pv.time.count(),
                              pv.get_nps(),
                              pv.moves);
    }
};
