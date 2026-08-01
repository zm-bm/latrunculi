#pragma once

#include <array>
#include <cassert>
#include <cstddef>

#include "core/move.hpp"

// Fixed-capacity list of generated moves.
class MoveList {
public:
    static constexpr std::size_t capacity = 256;

    MoveList() = default;
    MoveList(const MoveList& other);
    MoveList(MoveList&& other) noexcept;

    MoveList& operator=(const MoveList& other);
    MoveList& operator=(MoveList&& other) noexcept;

    void add(Move move);
    void add(Square from, Square to, MoveType type = BASIC_MOVE, PieceType promotion = KNIGHT);

    bool        empty() const { return last == moves.data(); }
    std::size_t size() const { return static_cast<std::size_t>(last - moves.data()); }

    Move*       begin() { return moves.data(); }
    const Move* begin() const { return moves.data(); }
    Move*       end() { return last; }
    const Move* end() const { return last; }

    Move&       operator[](int index) { return moves[index]; }
    const Move& operator[](int index) const { return moves[index]; }

private:
    void copy_active_range_from(const MoveList& other);

    // Fixed storage and cached end pointer; copies and moves rebind the pointer.
    std::array<Move, capacity> moves;
    Move*                      last{moves.data()};
};

inline MoveList::MoveList(const MoveList& other) {
    copy_active_range_from(other);
}

inline MoveList::MoveList(MoveList&& other) noexcept {
    copy_active_range_from(other);
}

inline MoveList& MoveList::operator=(const MoveList& other) {
    if (this == &other)
        return *this;

    copy_active_range_from(other);
    return *this;
}

inline MoveList& MoveList::operator=(MoveList&& other) noexcept {
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
    assert(size() < moves.size());
    *last++ = Move(from, to, type, promotion);
}

inline void MoveList::copy_active_range_from(const MoveList& other) {
    const auto active_count = other.size();
    for (std::size_t i = 0; i < active_count; ++i)
        moves[i] = other.moves[i];
    last = moves.data() + active_count;
}
