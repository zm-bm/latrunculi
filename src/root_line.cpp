#include "root_line.hpp"

namespace {

// Root-line ordering: completed depth, score, stable move bits.
bool is_better_root_line(const RootLine& candidate, const RootLine& current) {
    if (!candidate.usable_best_move())
        return false;

    if (!current.usable_best_move())
        return true;

    if (candidate.depth != current.depth)
        return candidate.depth > current.depth;

    if (candidate.value != current.value)
        return candidate.value > current.value;

    return candidate.best_move.bits < current.best_move.bits;
}

} // namespace

RootLine select_best_root_line(RootLine fallback, std::span<const RootLine> lines) {
    RootLine selected = fallback;
    for (const RootLine& line : lines) {
        if (is_better_root_line(line, selected))
            selected = line;
    }
    return selected;
}
