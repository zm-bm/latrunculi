#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "eval/nnue_network.hpp"

TEST(HalfKpNetworkTest, FrozenDecoderRejectsIncompatibleCorruptAndTruncatedBytes) {
    const auto embedded = nnue::embedded_network_bytes();
    std::vector<std::uint8_t> bytes(embedded.begin(), embedded.end());
    EXPECT_NO_THROW((void)nnue::decode(bytes));
    for (std::size_t offset : {0U, 4U, 8U, 189U, 21004993U, 21022696U}) {
        bytes[offset] ^= 1;
        EXPECT_THROW((void)nnue::decode(bytes), std::invalid_argument) << offset;
        bytes[offset] ^= 1;
    }
    bytes.pop_back();
    EXPECT_THROW((void)nnue::decode(bytes), std::invalid_argument);
    bytes.push_back(0);
    bytes.push_back(0);
    EXPECT_THROW((void)nnue::decode(bytes), std::invalid_argument);
}

TEST(HalfKpNetworkTest, EmbeddedNetworkMatchesEveryDecodedWeight) {
    const auto decoded = nnue::decode(nnue::embedded_network_bytes());
    const auto& loaded = nnue::network();
    EXPECT_EQ(nnue::network().weights.size(), nnue::inputs * nnue::hidden);
    EXPECT_EQ(decoded.bias, loaded.bias);
    EXPECT_EQ(decoded.weights, loaded.weights);
    EXPECT_EQ(decoded.first.bias, loaded.first.bias);
    EXPECT_EQ(decoded.first.weights, loaded.first.weights);
    EXPECT_EQ(decoded.second.bias, loaded.second.bias);
    EXPECT_EQ(decoded.second.weights, loaded.second.weights);
    EXPECT_EQ(decoded.output.bias, loaded.output.bias);
    EXPECT_EQ(decoded.output.weights, loaded.output.weights);
}
