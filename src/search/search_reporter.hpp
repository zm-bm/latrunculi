#pragma once

#include <string_view>

#include "core/move.hpp"
#include "core/types.hpp"

class Board;
struct RootLine;

// Synchronous search-result sink. The reporter must outlive every worker that
// references it; reporting failures intentionally propagate to the caller.
class SearchReporter {
public:
    virtual ~SearchReporter() = default;

    virtual void report_progress(const RootLine& line,
                                 const Board&    root_board,
                                 NodeCount       nodes,
                                 Milliseconds    time)       = 0;
    virtual void report_best_move(Move move)              = 0;
    virtual void report_diagnostic(std::string_view text) = 0;
};
