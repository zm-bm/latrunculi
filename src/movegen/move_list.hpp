#pragma once

#include <array>
#include <cassert>
#include <cstddef>

#include "core/move.hpp"

namespace movegen {

// Fixed-capacity list of generated moves.
class MoveList {
public:
    static constexpr std::size_t capacity = 256;

    // Initialize the active range only, including when constructing MoveList{}.
    MoveList() : last(moves.data()) {}

    // Inline storage uses the same active-prefix copy for lvalues and rvalues.
    MoveList(const MoveList& other) noexcept;
    MoveList& operator=(const MoveList& other) noexcept;

    void add(Move move);
    void add(Square from, Square to, MoveType type = BASIC_MOVE, PieceType promotion = KNIGHT);

    bool        empty() const { return last == moves.data(); }
    std::size_t size() const { return static_cast<std::size_t>(last - moves.data()); }

    Move*       begin() { return moves.data(); }
    const Move* begin() const { return moves.data(); }
    Move*       end() { return last; }
    const Move* end() const { return last; }

    Move& operator[](int index) {
        assert(index >= 0 && static_cast<std::size_t>(index) < size());
        return moves[index];
    }
    const Move& operator[](int index) const {
        assert(index >= 0 && static_cast<std::size_t>(index) < size());
        return moves[index];
    }

private:
    void copy_active_range_from(const MoveList& other) noexcept;

    // Only [moves.data(), last) is initialized. Copies rebind last to this storage.
    std::array<Move, capacity> moves;
    Move*                      last;
};

inline MoveList::MoveList(const MoveList& other) noexcept {
    copy_active_range_from(other);
}

inline MoveList& MoveList::operator=(const MoveList& other) noexcept {
    if (this == &other)
        return *this;

    copy_active_range_from(other);
    return *this;
}

inline void MoveList::add(Move move) {
    assert(size() < moves.size());
    *last++ = move;
}

inline void MoveList::add(Square from, Square to, MoveType type, PieceType promotion) {
    add(Move(from, to, type, promotion));
}

inline void MoveList::copy_active_range_from(const MoveList& other) noexcept {
    const auto active_count = other.size();
    for (std::size_t i = 0; i < active_count; ++i)
        moves[i] = other.moves[i];
    last = moves.data() + active_count;
}

} // namespace movegen
