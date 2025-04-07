#include <iostream>
#include <gtest/gtest.h>

#include "magics.hpp"
#include "zobrist.hpp"

class GlobalTestSetup : public ::testing::Environment {
   public:
    void SetUp() override {
        Magics::init();
        Zobrist::init();
    }
};

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new GlobalTestSetup());
    return RUN_ALL_TESTS();
}