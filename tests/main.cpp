#include <gtest/gtest.h>

#include "board/zobrist.hpp"
#include "core/attacks.hpp"

class GlobalTestSetup : public ::testing::Environment {
public:
    void SetUp() override {
        attacks::init();
        zob::init();
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new GlobalTestSetup());
    return RUN_ALL_TESTS();
}
