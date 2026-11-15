#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

#include "eval/nnue_network.hpp"

TEST(HalfKpNetworkTest, FrozenLoaderRejectsIncompatibleCorruptAndTruncatedFiles) {
    std::ifstream file(nnue::network_path(), std::ios::binary);
    ASSERT_TRUE(file);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
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

TEST(HalfKpNetworkTest, ExternalLoaderReadsPinnedFileAndRejectsMissingFiles) {
    const auto path = nnue::network_path();
    EXPECT_EQ(path.filename(), nnue::network_filename);
    EXPECT_EQ(nnue::network().weights.size(), nnue::inputs * nnue::hidden);
    EXPECT_EQ(nnue::load_network(path).bias, nnue::network().bias);
    EXPECT_THROW((void)nnue::load_network(path / "missing"), std::runtime_error);
}
