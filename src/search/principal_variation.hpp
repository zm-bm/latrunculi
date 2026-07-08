#pragma once

#include <algorithm>
#include <array>
#include <cassert>

#include "core/defs.hpp"
#include "core/move.hpp"

// Principal variation line built during search.
struct PrincipalVariation {
    PrincipalVariation() noexcept = default;

    PrincipalVariation(const PrincipalVariation& rhs) noexcept { *this = rhs; }
    PrincipalVariation(PrincipalVariation&& rhs) noexcept { *this = rhs; }

    PrincipalVariation& operator=(const PrincipalVariation& rhs) noexcept {
        if (this == &rhs)
            return *this;

        length = rhs.length;
        std::copy_n(rhs.moves.begin(), length, moves.begin());
        return *this;
    }

    PrincipalVariation& operator=(PrincipalVariation&& rhs) noexcept { return *this = rhs; }

    bool operator==(const PrincipalVariation& rhs) const noexcept {
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
    void update(Move head, const PrincipalVariation& child) noexcept {
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
    int                                          length{0};
};
