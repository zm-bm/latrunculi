#pragma once

#include "core/constants.hpp"
#include "core/move.hpp"
#include "search/pv.hpp"

namespace search {

// Root-search result passed between workers and final reporting.
struct RootLine {
    Move  root_move{NULL_MOVE};
    Score value{scores::draw};

    int  depth{0};
    bool completed{false};

    PV pv;

    void reset_attempt() noexcept {
        value     = -scores::inf;
        depth     = 0;
        completed = false;
        pv.clear();
    }

    void complete(int completed_depth, Score score, const PV& child_pv) {
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

} // namespace search
