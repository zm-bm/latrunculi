#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "search/root_line.hpp"
#include "search/search_reporter.hpp"

class RecordingSearchReporter final : public SearchReporter {
public:
    void report_progress(const RootLine& line, const Board&, NodeCount, Milliseconds) override {
        progress.push_back(line);
    }

    void report_best_move(Move move) override { best_moves.push_back(move); }

    void report_diagnostic(std::string_view text) override { diagnostics.emplace_back(text); }

    void clear() {
        progress.clear();
        best_moves.clear();
        diagnostics.clear();
    }

    std::vector<RootLine>    progress;
    std::vector<Move>        best_moves;
    std::vector<std::string> diagnostics;
};
