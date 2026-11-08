#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>

#include "core/constants.hpp"
#include "core/move.hpp"
#include "core/types.hpp"

namespace search {

// Principal variation line built during search.
struct PV {
    // Keep unused move slots uninitialized, including during value initialization.
    PV() noexcept
        : length(0) {}

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
    int size() const noexcept { return length; }

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
    int length;
};

// Small, copyable state saved when a search re-enters the same ply.
// Missing evaluations include PV and checked nodes; real-move flags exclude null edges.
struct SearchState {
    std::optional<Score> static_eval;
    Move excluded_move{NULL_MOVE};
    bool real_move{false};
    bool pv_enabled{false};

    // Inherited by descendants; excluded_move applies only at its own ply.
    bool singular_search_active{false};

    void start_singular_search(Move move) noexcept {
        excluded_move = move;
        pv_enabled = false;
        singular_search_active = true;
    }

    bool operator==(const SearchState&) const = default;
};

// One ply on the search path. Its index is fixed for the worker's lifetime.
struct SearchFrame {
    int ply{0};
    SearchState state;

    // Output for this ply. Disabled searches leave it alone; the caller clears it
    // when starting a new child buffer. Unused move slots remain uninitialized.
    PV pv;

    void reset() noexcept {
        state = {};
        pv.clear();
    }

    // Recursive output obeys this call's request. Child-buffer preparation
    // clears pv directly, before choosing whether the child will produce output.
    void clear_pv() noexcept {
        if (state.pv_enabled)
            pv.clear();
    }

    void update_pv(Move move, const PV& child) noexcept {
        if (state.pv_enabled)
            pv.update(move, child);
    }

    bool operator==(const SearchFrame&) const = default;
};

// Root-search result passed between workers and final reporting.
struct RootLine {
    Move root_move{NULL_MOVE};
    Score value{scores::draw};

    int depth{0};
    bool completed{false};

    PV pv;

    void reset_attempt() noexcept {
        value = -scores::inf;
        depth = 0;
        completed = false;
        pv.clear();
    }

    void complete(int completed_depth, Score score, const PV& child_pv) {
        value = score;
        depth = completed_depth;
        completed = true;
        pv.update(root_move, child_pv);
    }

    bool has_completed_depth() const noexcept { return completed && depth > 0; }
    bool has_root_move() const noexcept { return !root_move.is_null(); }
    bool usable_root_move() const noexcept { return has_completed_depth() && has_root_move(); }

    bool operator==(const RootLine& rhs) const noexcept = default;
};

// Root-line ordering: completed depth, score, stable move bits.
inline bool is_better_root_line(const RootLine& candidate, const RootLine& current) noexcept {
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
