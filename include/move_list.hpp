#pragma once

#include <array>
#include <cassert>
#include <cstddef>

#include "defs.hpp"
#include "move.hpp"

class MoveList {
public:
    MoveList() : last(moves.data()) {}
    MoveList(const MoveList& other);
    MoveList(MoveList&& other) noexcept;

    MoveList& operator=(const MoveList& other);
    MoveList& operator=(MoveList&& other) noexcept;

    void add(Move move);
    void add(Square from, Square to, MoveType mtype = BASIC_MOVE, PieceType prom = KNIGHT);

    Move*       begin() { return moves.data(); }
    const Move* begin() const { return moves.data(); }
    Move*       end() { return last; }
    const Move* end() const { return last; }
    bool        empty() const { return last == moves.data(); }
    std::size_t size() const { return static_cast<std::size_t>(last - moves.data()); }
    Move&       operator[](int index) { return moves[index]; }
    const Move& operator[](int index) const { return moves[index]; }
    void        clear() { last = moves.data(); }

private:
    void copy_active_range_from(const MoveList& other);

    std::array<Move, MAX_MOVES> moves;
    Move*                       last;
};

inline MoveList::MoveList(const MoveList& other) : last(moves.data()) {
    *this = other;
}

inline MoveList::MoveList(MoveList&& other) noexcept : last(moves.data()) {
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

inline void MoveList::copy_active_range_from(const MoveList& other) {
    const auto count = other.size();
    for (std::size_t i = 0; i < count; ++i)
        moves[i] = other.moves[i];
    last = moves.data() + count;
}

inline void MoveList::add(Move move) {
    assert(size() < moves.size());
    *last++ = move;
}

inline void MoveList::add(Square from, Square to, MoveType mtype, PieceType prom) {
    assert(size() < moves.size());
    *last++ = Move(from, to, mtype, prom);
}
