#pragma once

#include <sstream>

#include "defs.hpp"

struct Move {
    constexpr Move() = default;
    constexpr Move(Square from, Square to, MoveType m_type = BASIC_MOVE, PieceType prom = KNIGHT);

    Square      from() const { return unpack_from(value); }
    Square      to() const { return unpack_to(value); }
    MoveType    type() const { return unpack_type(value); }
    PieceType   prom_piece() const { return unpack_prom(value); }
    std::string str() const;

    bool is_null() const { return value == 0; }
    bool operator==(const Move& rhs) const { return value == rhs.value; }

    friend std::ostream& operator<<(std::ostream& os, const Move& move);

    static constexpr uint16_t pack(Square from, Square to, MoveType moveType, PieceType promoPiece);
    static constexpr Square   unpack_from(uint16_t packed);
    static constexpr Square   unpack_to(uint16_t packed);
    static constexpr MoveType unpack_type(uint16_t packed);
    static constexpr PieceType unpack_prom(uint16_t packed);

    static constexpr uint16_t move_mask  = 0x3F;
    static constexpr uint16_t flag_mask  = 0x03;
    static constexpr uint16_t to_shift   = 6;
    static constexpr uint16_t type_shift = 12;
    static constexpr uint16_t prom_shift = 14;

    /**
     * move encoding:
     * 6 bits for from square (0-5)
     * 6 bits for to square (6-11)
     * 2 bits for move type (12-13)
     * 2 bits for promotion piece (14-15)
     */
    uint16_t value = {0};

    /// move priority used for sorting
    uint16_t priority = {0};
};

constexpr Move NULL_MOVE{};

constexpr Move::Move(Square from, Square to, MoveType m_type, PieceType prom)
    : value{pack(from, to, m_type, prom)},
      priority(0) {}

constexpr uint16_t Move::pack(Square from, Square to, MoveType m_type, PieceType prom) {
    return (from & move_mask) |                              // 6 bits for from
           ((to & move_mask) << to_shift) |                  // 6 bits for to
           ((int(m_type) & flag_mask) << type_shift) |       // 2 bits for move type
           ((int(prom - KNIGHT) & flag_mask) << prom_shift); // 2 bits for promos
}

constexpr Square Move::unpack_from(uint16_t packed) {
    return static_cast<Square>(packed & move_mask);
}

constexpr Square Move::unpack_to(uint16_t packed) {
    return static_cast<Square>((packed >> to_shift) & move_mask);
}

constexpr MoveType Move::unpack_type(uint16_t packed) {
    return static_cast<MoveType>((packed >> type_shift) & flag_mask);
}

constexpr PieceType Move::unpack_prom(uint16_t packed) {
    return static_cast<PieceType>(((packed >> prom_shift) & flag_mask) + KNIGHT);
}
