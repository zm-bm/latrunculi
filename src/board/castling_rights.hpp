#pragma once

#include <cstdint>

enum CastlingRights : std::uint8_t {
    NO_CASTLE   = 0b0000,
    B_QUEENSIDE = 0b0001,
    B_KINGSIDE  = 0b0010,
    W_QUEENSIDE = 0b0100,
    W_KINGSIDE  = 0b1000,
    B_CASTLE    = B_KINGSIDE | B_QUEENSIDE,
    W_CASTLE    = W_KINGSIDE | W_QUEENSIDE,
    ALL_CASTLE  = B_CASTLE | W_CASTLE,
};

constexpr CastlingRights operator~(CastlingRights cr) noexcept {
    return static_cast<CastlingRights>(~std::uint8_t(cr));
}

constexpr CastlingRights operator|(CastlingRights lhs, CastlingRights rhs) noexcept {
    return CastlingRights(std::uint8_t(lhs) | std::uint8_t(rhs));
}

constexpr CastlingRights operator&(CastlingRights lhs, CastlingRights rhs) noexcept {
    return CastlingRights(std::uint8_t(lhs) & std::uint8_t(rhs));
}

constexpr CastlingRights& operator|=(CastlingRights& lhs, CastlingRights rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr CastlingRights& operator&=(CastlingRights& lhs, CastlingRights rhs) noexcept {
    lhs = lhs & rhs;
    return lhs;
}
