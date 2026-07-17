#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <type_traits>

#include "core/piece.hpp"
#include "core/square.hpp"
#include "core/types.hpp"

// Encodes special move handling beyond a normal from-to move.
enum MoveType : std::uint8_t { BASIC_MOVE, MOVE_PROM, MOVE_EP, MOVE_CASTLE };

// Packed 16-bit move storage.
using MoveBits = std::uint16_t;

// Value type for chess moves, stored entirely in MoveBits.
struct Move {
    constexpr Move() = default;

    // Packs a move; promotion is meaningful only for MOVE_PROM.
    constexpr Move(Square    from,
                   Square    to,
                   MoveType  move_type = BASIC_MOVE,
                   PieceType promotion = KNIGHT);

    // Decoded source square.
    constexpr Square from() const noexcept { return unpack_from(bits); }

    // Decoded destination square.
    constexpr Square to() const noexcept { return unpack_to(bits); }

    // Decoded move kind.
    constexpr MoveType type() const noexcept { return unpack_type(bits); }

    // Decoded promotion result; defaults to KNIGHT for non-promotions.
    constexpr PieceType prom_piece() const noexcept { return unpack_prom(bits); }

    // UCI-style move text, or "none" for NULL_MOVE.
    std::string str() const;

    // Null move sentinel check.
    constexpr bool is_null() const noexcept { return bits == 0; }

    // Exact packed-value equality.
    constexpr bool operator==(const Move& rhs) const noexcept { return bits == rhs.bits; }

    // Streams UCI-style move text.
    friend std::ostream& operator<<(std::ostream& os, const Move& move);

    // Packs move fields into the 16-bit layout.
    static constexpr MoveBits
    pack(Square from, Square to, MoveType move_type, PieceType promotion) noexcept;

    // Extractors for the 16-bit move layout.
    static constexpr Square    unpack_from(MoveBits packed) noexcept;
    static constexpr Square    unpack_to(MoveBits packed) noexcept;
    static constexpr MoveType  unpack_type(MoveBits packed) noexcept;
    static constexpr PieceType unpack_prom(MoveBits packed) noexcept;

    // Bit masks and shifts for MoveBits.
    static constexpr MoveBits move_mask  = 0x3F;
    static constexpr MoveBits flag_mask  = 0x03;
    static constexpr MoveBits to_shift   = 6;
    static constexpr MoveBits type_shift = 12;
    static constexpr MoveBits prom_shift = 14;

    // MoveBits layout:
    // 6 bits for from square (0-5), 6 bits for to square (6-11),
    // 2 bits for move type (12-13), 2 bits for promotion piece (14-15).
    MoveBits bits = {0};
};

// Null move sentinel.
constexpr Move NULL_MOVE{};

constexpr Move::Move(Square from, Square to, MoveType move_type, PieceType promotion)
    : bits{pack(from, to, move_type, promotion)} {}

constexpr MoveBits
Move::pack(Square from, Square to, MoveType move_type, PieceType promotion) noexcept {
    return (from & move_mask) | ((to & move_mask) << to_shift) |
           ((int(move_type) & flag_mask) << type_shift) |
           ((int(promotion - KNIGHT) & flag_mask) << prom_shift);
}

constexpr Square Move::unpack_from(MoveBits packed) noexcept {
    return static_cast<Square>(packed & move_mask);
}

constexpr Square Move::unpack_to(MoveBits packed) noexcept {
    return static_cast<Square>((packed >> to_shift) & move_mask);
}

constexpr MoveType Move::unpack_type(MoveBits packed) noexcept {
    return static_cast<MoveType>((packed >> type_shift) & flag_mask);
}

constexpr PieceType Move::unpack_prom(MoveBits packed) noexcept {
    return static_cast<PieceType>(((packed >> prom_shift) & flag_mask) + KNIGHT);
}

static_assert(sizeof(Move) == sizeof(MoveBits));
static_assert(std::is_trivially_copyable_v<Move>);
