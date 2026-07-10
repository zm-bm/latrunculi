#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <type_traits>

#include "core/piece.hpp"
#include "core/square.hpp"
#include "core/types.hpp"

enum MoveType : uint8_t { BASIC_MOVE, MOVE_PROM, MOVE_EP, MOVE_CASTLE };

struct Move {
    constexpr Move() = default;
    constexpr Move(Square    from,
                   Square    to,
                   MoveType  move_type = BASIC_MOVE,
                   PieceType promotion = KNIGHT);

    constexpr Square    from() const noexcept { return unpack_from(bits); }
    constexpr Square    to() const noexcept { return unpack_to(bits); }
    constexpr MoveType  type() const noexcept { return unpack_type(bits); }
    constexpr PieceType prom_piece() const noexcept { return unpack_prom(bits); }
    std::string         str() const;

    constexpr bool is_null() const noexcept { return bits == 0; }
    constexpr bool operator==(const Move& rhs) const noexcept { return bits == rhs.bits; }

    friend std::ostream& operator<<(std::ostream& os, const Move& move);

    static constexpr uint16_t
    pack(Square from, Square to, MoveType move_type, PieceType promotion) noexcept;
    static constexpr Square    unpack_from(uint16_t packed) noexcept;
    static constexpr Square    unpack_to(uint16_t packed) noexcept;
    static constexpr MoveType  unpack_type(uint16_t packed) noexcept;
    static constexpr PieceType unpack_prom(uint16_t packed) noexcept;

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
    uint16_t bits = {0};
};

constexpr Move NULL_MOVE{};

using MoveScore = int;

struct ScoredMove {
    Move      move;
    MoveScore score;
};

constexpr Move::Move(Square from, Square to, MoveType move_type, PieceType promotion)
    : bits{pack(from, to, move_type, promotion)} {}

constexpr uint16_t
Move::pack(Square from, Square to, MoveType move_type, PieceType promotion) noexcept {
    return (from & move_mask) |                                   // 6 bits for from
           ((to & move_mask) << to_shift) |                       // 6 bits for to
           ((int(move_type) & flag_mask) << type_shift) |         // 2 bits for move type
           ((int(promotion - KNIGHT) & flag_mask) << prom_shift); // 2 bits for promos
}

constexpr Square Move::unpack_from(uint16_t packed) noexcept {
    return static_cast<Square>(packed & move_mask);
}

constexpr Square Move::unpack_to(uint16_t packed) noexcept {
    return static_cast<Square>((packed >> to_shift) & move_mask);
}

constexpr MoveType Move::unpack_type(uint16_t packed) noexcept {
    return static_cast<MoveType>((packed >> type_shift) & flag_mask);
}

constexpr PieceType Move::unpack_prom(uint16_t packed) noexcept {
    return static_cast<PieceType>(((packed >> prom_shift) & flag_mask) + KNIGHT);
}

static_assert(sizeof(Move) == sizeof(uint16_t));
static_assert(std::is_trivially_copyable_v<Move>);
static_assert(std::is_trivially_copyable_v<ScoredMove>);
