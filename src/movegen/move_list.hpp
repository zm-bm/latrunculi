#pragma once

#include <array>
#include <cassert>
#include <cstddef>

#include "core/move.hpp"

class MoveList {
public:
    static constexpr std::size_t capacity = 256;

    MoveList() = default;

    void add(Move move);
    void add(Square from, Square to, MoveType mtype = BASIC_MOVE, PieceType prom = KNIGHT);
    void clear();

    bool        empty() const;
    std::size_t size() const;

    Move*       begin();
    const Move* begin() const;
    Move*       end();
    const Move* end() const;

    Move&       operator[](int index);
    const Move& operator[](int index) const;

    MoveList(const MoveList& other);
    MoveList(MoveList&& other) noexcept;

    MoveList& operator=(const MoveList& other);
    MoveList& operator=(MoveList&& other) noexcept;

private:
    void copy_active_range_from(const MoveList& other);

    std::array<Move, capacity> moves;
    Move*                       last{moves.data()};
};

inline void MoveList::add(Move move) {
    assert(size() < moves.size());
    *last++ = move;
}

inline void MoveList::add(Square from, Square to, MoveType mtype, PieceType prom) {
    assert(size() < moves.size());
    *last++ = Move(from, to, mtype, prom);
}

inline void MoveList::clear() {
    last = moves.data();
}

inline bool MoveList::empty() const {
    return last == moves.data();
}

inline std::size_t MoveList::size() const {
    return static_cast<std::size_t>(last - moves.data());
}

inline Move* MoveList::begin() {
    return moves.data();
}

inline const Move* MoveList::begin() const {
    return moves.data();
}

inline Move* MoveList::end() {
    return last;
}

inline const Move* MoveList::end() const {
    return last;
}

inline Move& MoveList::operator[](int index) {
    return moves[index];
}

inline const Move& MoveList::operator[](int index) const {
    return moves[index];
}

inline MoveList::MoveList(const MoveList& other) {
    *this = other;
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

inline void MoveList::copy_active_range_from(const MoveList& other) {
    const auto active_count = other.size();
    for (std::size_t i = 0; i < active_count; ++i)
        moves[i] = other.moves[i];
    last = moves.data() + active_count;
}
