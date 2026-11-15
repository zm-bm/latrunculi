#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "eval/nnue.hpp"

namespace nnue {

inline constexpr char network_filename[] = "nn-82215d0fd0df.nnue";
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
    Layer<512, 32> first;
    Layer<32, 32> second;
    Layer<32, 1> output;
};

Network decode(std::span<const std::uint8_t> bytes);
std::filesystem::path network_path();
Network load_network(const std::filesystem::path& path);
const Network& network();

} // namespace nnue
