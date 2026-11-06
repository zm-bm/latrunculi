#pragma once

#include <optional>

#include "core/move.hpp"
#include "core/types.hpp"

namespace search {

// State for one ply on the current search path. Missing evaluations include PV
// and checked nodes; real-move flags exclude trends crossing null moves.
struct SearchFrame {
    std::optional<Score> static_eval;
    Move                 excluded_move{NULL_MOVE};
    bool                 real_move{false};

    bool operator==(const SearchFrame&) const = default;
};

} // namespace search
