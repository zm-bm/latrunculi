#pragma once

#include <array>
#include <cstdint>

#include "core/piece.hpp"
#include "core/square.hpp"

class Board;

namespace nnue {
inline constexpr int inputs = 41024;
inline constexpr int hidden = 256;
inline constexpr int static_limit = 30000;

enum class Backend { Scalar, Avx2 };

struct Network;

// Initialize the automatic backend and fixed network before accepting commands.
void initialize();
Backend resolve_backend(bool supports_avx2) noexcept;
Backend backend();
bool avx2_available() noexcept;
const char* backend_name();

struct alignas(64) Accumulators {
    std::array<std::array<std::int16_t, hidden>, N_COLORS> values{};
    bool operator==(const Accumulators&) const = default;
};

// Legacy HalfKP rotates both the piece square and the friendly king for Black.
constexpr int feature(Color perspective, Square king, Color color, PieceType piece, Square square) {
    const int rotation = perspective == BLACK ? 63 : 0;
    return (int(square) ^ rotation)
        + 1
        + (int(piece) - 1) * 128
        + (color != perspective ? 64 : 0)
        + 641 * (int(king) ^ rotation);
}

void refresh(const Board&, Accumulators&, Color perspective, Backend);
Accumulators refresh(const Board&, Backend);
Accumulators refresh(const Board&);
void update(Accumulators&,
            Color perspective,
            Square king,
            Color color,
            PieceType,
            Square square,
            int sign);

struct Trace {
    std::array<std::uint8_t, 512> transformed{};
    std::array<std::int32_t, 32> first_sums{}, second_sums{};
    std::array<std::uint8_t, 32> first{}, second{};
    std::int32_t numerator{};
    int raw{};
    bool operator==(const Trace&) const = default;
};
Trace trace(const Network&, const Accumulators&, Color turn, Backend);
int raw(const Accumulators&, Color turn);
Score adapt(int raw) noexcept;
Score evaluate(const Accumulators&, Color turn);
} // namespace nnue
