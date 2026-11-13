#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "core/piece.hpp"
#include "core/square.hpp"

class Board;

namespace nnue {
inline constexpr int inputs = 768;
inline constexpr int hidden = 256;
inline constexpr int hidden_scale = 255;
inline constexpr int output_scale = 4096;
inline constexpr Score static_limit = 30000;

struct Network {
    std::array<std::array<std::int16_t, hidden>, inputs> weights;
    std::array<std::int16_t, hidden> bias;
    std::array<std::int16_t, 2 * hidden> output;
    std::int64_t output_bias;
    int calibration;
};

// Versioned little-endian file with FNV-1a-64 over header metadata plus payload.
// Artifacts additionally retain SHA256. Parsing proves bounds before evaluation.
Network decode(std::span<const std::uint8_t> bytes);
const Network& network();
const char* network_sha256();
constexpr int feature(Color perspective, Color color, PieceType piece, Square square) {
    return ((color != perspective ? 6 : 0) + piece - 1) * 64
        + (static_cast<int>(square) ^ (perspective == BLACK ? 56 : 0));
}

struct Accumulators {
    std::array<std::array<std::int32_t, hidden>, N_COLORS> values{};
    void reset() noexcept;
    void update(Color color, PieceType piece, Square square, int sign) noexcept;
    void move(Color color, PieceType piece, Square from, Square to) noexcept;
    bool operator==(const Accumulators&) const = default;
};

Accumulators refresh(const Board& board);
Score evaluate(const Network& weights, const Accumulators& state, Color turn) noexcept;
Score evaluate(const Accumulators& state, Color turn) noexcept;
} // namespace nnue
