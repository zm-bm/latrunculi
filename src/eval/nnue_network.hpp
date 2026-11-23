#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "eval/nnue.hpp"

namespace nnue {

inline constexpr char network_sha256[] =
    "82215d0fd0df3cde17fc47ac024c5b5736823ec390200abdd705c44660c69ee4";

template <int In, int Out>
struct Layer {
    std::array<std::int32_t, Out> bias{};
    alignas(64) std::array<std::int8_t, In * Out> weights{};
};

struct Network {
    std::array<std::int16_t, hidden> bias{};
    std::vector<std::int16_t> weights;
    Layer<transformed_size, layer_width> first;
    Layer<layer_width, layer_width> second;
    Layer<layer_width, 1> output;
};

Network decode(std::span<const std::uint8_t> bytes);
std::span<const std::uint8_t> embedded_network_bytes() noexcept;
const Network& network();

} // namespace nnue
