#pragma once

#include <optional>

#include "core/move.hpp"
#include "core/types.hpp"
#include "search/pv.hpp"

namespace search {

// Compile-time search policy, independent of a call's PV-output request.
enum class NodeType { Pv, NonPv };

// One ply on the search path. Its index is fixed for the worker's lifetime.
// Missing evaluations include PV and checked nodes; real-move flags exclude null edges.
struct SearchFrame {
    int                  ply{0};
    std::optional<Score> static_eval;
    Move                 excluded_move{NULL_MOVE};
    bool                 real_move{false};
    bool                 pv_enabled{false};

    // Inherited by descendants; excluded_move applies only at its own ply.
    bool singular_search_active{false};

    // Output for this ply. Disabled searches leave it alone; the caller clears it
    // when starting a new child buffer. Unused move slots remain uninitialized.
    PV pv;

    bool operator==(const SearchFrame&) const = default;
};

} // namespace search
