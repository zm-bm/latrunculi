#pragma once

#include <cstdint>

enum CastleRights : std::uint8_t {
    NO_CASTLE   = 0b0000,
    B_QUEENSIDE = 0b0001,
    B_KINGSIDE  = 0b0010,
    W_QUEENSIDE = 0b0100,
    W_KINGSIDE  = 0b1000,
    B_CASTLE    = B_KINGSIDE | B_QUEENSIDE,
    W_CASTLE    = W_KINGSIDE | W_QUEENSIDE,
    ALL_CASTLE  = B_CASTLE | W_CASTLE,
};

constexpr CastleRights operator~(CastleRights cr) noexcept {
    return static_cast<CastleRights>(~std::uint8_t(cr));
}

constexpr CastleRights operator|(CastleRights lhs, CastleRights rhs) noexcept {
    return CastleRights(std::uint8_t(lhs) | std::uint8_t(rhs));
}

constexpr CastleRights operator&(CastleRights lhs, CastleRights rhs) noexcept {
    return CastleRights(std::uint8_t(lhs) & std::uint8_t(rhs));
}

constexpr CastleRights& operator|=(CastleRights& lhs, CastleRights rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr CastleRights& operator&=(CastleRights& lhs, CastleRights rhs) noexcept {
    lhs = lhs & rhs;
    return lhs;
}
