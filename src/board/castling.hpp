#pragma once

#include <cstdint>

#include "core/types.hpp"

enum CastleSide { CASTLE_KINGSIDE, CASTLE_QUEENSIDE, N_CASTLES };

enum CastleRights : uint8_t {
    NO_CASTLE   = 0b0000,
    B_QUEENSIDE = 0b0001,
    B_KINGSIDE  = 0b0010,
    W_QUEENSIDE = 0b0100,
    W_KINGSIDE  = 0b1000,
    B_CASTLE    = B_KINGSIDE | B_QUEENSIDE,
    W_CASTLE    = W_KINGSIDE | W_QUEENSIDE,
    ALL_CASTLE  = B_CASTLE | W_CASTLE,
};

constexpr CastleRights operator~(CastleRights cr) {
    return static_cast<CastleRights>(~uint8_t(cr));
}

constexpr CastleRights operator|(CastleRights lhs, CastleRights rhs) {
    return CastleRights(uint8_t(lhs) | uint8_t(rhs));
}

constexpr CastleRights operator&(CastleRights lhs, CastleRights rhs) {
    return CastleRights(uint8_t(lhs) & uint8_t(rhs));
}

constexpr CastleRights& operator|=(CastleRights& lhs, CastleRights rhs) {
    lhs = lhs | rhs;
    return lhs;
}

constexpr CastleRights& operator&=(CastleRights& lhs, CastleRights rhs) {
    lhs = lhs & rhs;
    return lhs;
}

namespace castle {

constexpr Square rook_from[N_CASTLES][N_COLORS] = {
    {H8, H1}, // kingside
    {A8, A1}, // queenside
};

constexpr Square rook_to[N_CASTLES][N_COLORS] = {
    {F8, F1}, // kingside
    {D8, D1}, // queenside
};

constexpr uint64_t path[N_CASTLES][N_COLORS]{
    {0x6000000000000000ull, 0x0000000000000060ull}, // kingside
    {0x0E00000000000000ull, 0x000000000000000Eull}, // queenside
};

constexpr uint64_t kingpath[N_CASTLES][N_COLORS]{
    {0x7000000000000000ull, 0x0000000000000070ull}, // kingside
    {0x1C00000000000000ull, 0x000000000000001Cull}, // queenside
};

}; // namespace castle
