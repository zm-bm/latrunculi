#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>

#include "defs.hpp"
#include "move.hpp"
#include "uci_options.hpp"

class Board;
struct RootLine;

namespace uci {

// UCI wire-format helpers.
std::string format_uci_move(Move move);
std::string format_option(std::string_view name, const SpinOption& opt);
std::string format_option(std::string_view name, const CheckOption& opt);
std::string format_option(std::string_view name, const ButtonOption& opt);
std::string format_identification(const uci::Options& options);
std::string format_ready();
std::string format_bestmove(Move move);
std::string format_info_string(std::string_view str);

// UCI stdout writer and local debug-console stderr writer.
class Writer {
public:
    explicit Writer(std::ostream& out, std::ostream& err) : out(out), err(err) {}

    void help() const;
    void identify(const uci::Options& options) const;
    void ready() const;
    void bestmove(Move move) const;
    void search_info(const RootLine& line,
                     const Board&    root_board,
                     uint64_t        nodes,
                     Milliseconds    time) const;
    void info_string(std::string_view str) const;
    void debug_text(std::string_view str) const;

    template <typename T>
    void debug(T&& obj) const;

private:
    std::ostream& out;
    std::ostream& err;
};

} // namespace uci
