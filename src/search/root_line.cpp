#include "search/root_line.hpp"

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

RootLine select_best_root_line(RootLine fallback, std::span<const RootLine> lines) {
    RootLine selected = fallback;
    for (const RootLine& line : lines) {
        if (is_better_root_line(line, selected))
            selected = line;
    }
    return selected;
}
