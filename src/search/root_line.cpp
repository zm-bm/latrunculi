#include "search/root_line.hpp"

namespace search {

// Root-line ordering: completed depth, score, stable move bits.
bool is_better_root_line(const RootLine& candidate, const RootLine& current) noexcept {
    if (!candidate.usable_root_move())
        return false;

    if (!current.usable_root_move())
        return true;

    if (candidate.depth != current.depth)
        return candidate.depth > current.depth;

    if (candidate.value != current.value)
        return candidate.value > current.value;

    return candidate.root_move.bits < current.root_move.bits;
}

} // namespace search
