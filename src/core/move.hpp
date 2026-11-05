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
    // Like a scalar: Move{} is null; a bare Move must be assigned before reading.
    constexpr Move() = default;

    // Packs a move; promotion is meaningful only for MOVE_PROM.
    constexpr Move(Square    from,
                   Square    to,
                   MoveType  move_type = BASIC_MOVE,
                   PieceType promotion = KNIGHT)
        : bits{static_cast<MoveBits>((from & move_mask) | ((to & move_mask) << to_shift)
                                     | ((int(move_type) & flag_mask) << type_shift)
                                     | ((int(promotion - KNIGHT) & flag_mask) << prom_shift))} {}

    // Decoded source square.
    constexpr Square from() const noexcept { return static_cast<Square>(bits & move_mask); }

    // Decoded destination square.
    constexpr Square to() const noexcept {
        return static_cast<Square>((bits >> to_shift) & move_mask);
    }

    // Decoded move kind.
    constexpr MoveType type() const noexcept {
        return static_cast<MoveType>((bits >> type_shift) & flag_mask);
    }

    // Decoded promotion result; defaults to KNIGHT for non-promotions.
    constexpr PieceType prom_piece() const noexcept {
        return static_cast<PieceType>(((bits >> prom_shift) & flag_mask) + KNIGHT);
    }

    // UCI-style move text, or "none" for NULL_MOVE.
    std::string str() const;

    // Null move sentinel check.
    constexpr bool is_null() const noexcept { return bits == 0; }

    // Exact packed-value equality.
    constexpr bool operator==(const Move& rhs) const noexcept { return bits == rhs.bits; }

    // Streams UCI-style move text.
    friend std::ostream& operator<<(std::ostream& os, const Move& move);

    // MoveBits layout:
    // 6 bits for from square (0-5), 6 bits for to square (6-11),
    // 2 bits for move type (12-13), 2 bits for promotion piece (14-15).
    MoveBits bits;

private:
    // Bit masks and shifts for MoveBits.
    static constexpr MoveBits move_mask  = 0x3F;
    static constexpr MoveBits flag_mask  = 0x03;
    static constexpr MoveBits to_shift   = 6;
    static constexpr MoveBits type_shift = 12;
    static constexpr MoveBits prom_shift = 14;
};

// Null move sentinel.
constexpr Move NULL_MOVE{};

static_assert(sizeof(Move) == sizeof(MoveBits));
static_assert(std::is_trivially_default_constructible_v<Move>);
static_assert(std::is_trivially_copyable_v<Move>);
