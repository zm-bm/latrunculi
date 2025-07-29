#include <gtest/gtest.h>

#include "magic.hpp"
#include "zobrist.hpp"

class GlobalTestSetup : public ::testing::Environment {
public:
    void SetUp() override {
        magic::init();
        zob::init();
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new GlobalTestSetup());
    return RUN_ALL_TESTS();
}
