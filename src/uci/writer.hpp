#pragma once

#include <mutex>
#include <ostream>
#include <string_view>

#include "core/move.hpp"
#include "core/types.hpp"
#include "search/search_reporter.hpp"

class Board;
struct RootLine;

namespace uci {

struct Options;

// UCI stdout writer and diagnostic stderr writer.
class Writer final : public SearchReporter {
public:
    explicit Writer(std::ostream& output_stream, std::ostream& diagnostic_stream)
        : output(output_stream),
          diagnostics(diagnostic_stream) {}
    Writer(const Writer&)            = delete;
    Writer& operator=(const Writer&) = delete;
    Writer(Writer&&)                 = delete;
    Writer& operator=(Writer&&)      = delete;

    void help() const;
    void identify(const Options& options) const;
    void ready() const;
    void info_string(std::string_view str) const;
    void diagnostic_line(std::string_view text) const;
    void diagnostic_text(std::string_view text) const;

    void report_progress(const RootLine& line,
                         const Board&    root_board,
                         NodeCount       nodes,
                         Milliseconds    time) override;
    void report_best_move(Move move) override;
    void report_diagnostic(std::string_view text) override;

private:
    void write_text(std::ostream& stream, std::string_view text) const;
    void write_line(std::ostream& stream, std::string_view text) const;

    std::ostream& output;
    std::ostream& diagnostics;

    mutable std::mutex output_mutex;
};

} // namespace uci
