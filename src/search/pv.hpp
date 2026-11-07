#pragma once

#include <algorithm>
#include <array>
#include <cassert>

#include "core/constants.hpp"
#include "core/move.hpp"

namespace search {

// Principal variation line built during search.
struct PV {
    // Keep unused move slots uninitialized, including during value initialization.
    PV() noexcept : length(0) {}

    PV(const PV& rhs) noexcept { *this = rhs; }
    PV(PV&& rhs) noexcept { *this = rhs; }

    PV& operator=(const PV& rhs) noexcept {
        if (this == &rhs)
            return *this;

        length = rhs.length;
        std::copy_n(rhs.moves.begin(), length, moves.begin());
        return *this;
    }

    PV& operator=(PV&& rhs) noexcept { return *this = rhs; }

    bool operator==(const PV& rhs) const noexcept {
        if (length != rhs.length)
            return false;

        return std::equal(moves.begin(), moves.begin() + length, rhs.moves.begin());
    }

    void clear() noexcept { length = 0; }
    bool empty() const noexcept { return length == 0; }
    int  size() const noexcept { return length; }

    Move front() const noexcept {
        assert(!empty());
        return move_at(0);
    }

    Move move_at(int index) const noexcept {
        assert(index >= 0 && index < length);
        return moves[index];
    }

    // Replace this PV with head followed by child.
    void update(Move head, const PV& child) noexcept {
        assert(this != &child);

        const int child_length =
            child.length < engine::max_search_ply ? child.length : engine::max_search_ply;

        moves[0] = head;
        std::copy_n(child.moves.begin(), child_length, moves.begin() + 1);
        length = child_length + 1;
    }

private:
    // Only [0, length) is valid PV data.
    std::array<Move, engine::max_search_ply + 1> moves;
    int                                          length;
};

} // namespace search
