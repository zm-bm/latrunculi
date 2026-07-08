#pragma once

#include <span>

#include "core/constants.hpp"
#include "core/move.hpp"
#include "search/principal_variation.hpp"

// Root-search result passed between workers and final reporting.
struct RootLine {
    Move      root_move{NULL_MOVE};
    EvalValue value{eval_value::draw};

    int  depth{0};
    bool completed{false};

    PrincipalVariation pv;

    void reset_attempt() noexcept {
        value     = -eval_value::inf;
        depth     = 0;
        completed = false;
        pv.clear();
    }

    void complete(int completed_depth, EvalValue score, const PrincipalVariation& child_pv) {
        value     = score;
        depth     = completed_depth;
        completed = true;
        pv.update(root_move, child_pv);
    }

    bool has_completed_depth() const noexcept { return completed && depth > 0; }
    bool has_root_move() const noexcept { return !root_move.is_null(); }
    bool usable_root_move() const noexcept { return has_completed_depth() && has_root_move(); }

    bool operator==(const RootLine& rhs) const noexcept = default;
};

bool is_better_root_line(const RootLine& candidate, const RootLine& current) noexcept;

// Pick the best usable completed line, falling back to the caller's line.
RootLine select_best_root_line(RootLine fallback, std::span<const RootLine> lines);
