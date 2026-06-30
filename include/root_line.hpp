#pragma once

#include <span>

#include "defs.hpp"
#include "move.hpp"
#include "principal_variation.hpp"

// Root-search result passed between workers and final reporting.
struct RootLine {
    Move best_move{NULL_MOVE};
    int  value{DRAW_VALUE};

    int  depth{0};
    bool completed{false};

    PrincipalVariation pv;

    bool completed_depth() const noexcept { return completed && depth > 0; }
    bool has_best_move() const noexcept { return !best_move.is_null(); }
    bool usable_best_move() const noexcept { return completed_depth() && has_best_move(); }
};

bool is_better_root_line(const RootLine& candidate, const RootLine& current) noexcept;

// Pick the best usable completed line, falling back to the caller's line.
RootLine select_best_root_line(RootLine fallback, std::span<const RootLine> lines);
