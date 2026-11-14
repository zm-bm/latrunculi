#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "core/piece.hpp"
#include "core/square.hpp"

class Board;

namespace nnue {
inline constexpr int inputs = 41024;
inline constexpr int hidden = 256;
inline constexpr int static_limit = 30000;
inline constexpr char network_sha256[] =
    "82215d0fd0df3cde17fc47ac024c5b5736823ec390200abdd705c44660c69ee4";

enum class Backend { Scalar, Avx2 };
enum class BackendPreference { Auto, Scalar, Avx2 };
Backend resolve_backend(BackendPreference, bool supports_avx2);
Backend backend();
bool avx2_available() noexcept;
const char* backend_name();

template <int In, int Out>
struct Layer {
    std::array<std::int32_t, Out> bias{};
    alignas(64) std::array<std::int8_t, In * Out> weights{};
};

struct Network {
    std::array<std::int16_t, hidden> bias{};
    std::vector<std::int16_t> weights;
    Layer<512, 32> first;
    Layer<32, 32> second;
    Layer<32, 1> output;
};

Network decode(std::span<const std::uint8_t> bytes);
std::span<const std::uint8_t> embedded_network();
const Network& network();

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
