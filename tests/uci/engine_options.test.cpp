#include "support/engine_test_fixture.hpp"

#include <string>

#include "core/move.hpp"
#include "search/tt.hpp"
#include "gtest/gtest.h"

class EngineOptionsTest : public EngineTest {
protected:
    int  hash_option_mb() const { return options().hash.value; }
    int  thread_option_count() const { return options().threads.value; }
    bool ponder_enabled() const { return options().ponder.value; }
};

TEST_F(EngineOptionsTest, ThreadOptionCommitsOnlyAfterSuccessfulResize) {
    EXPECT_TRUE(execute("setoption name tHrEaDs value 2"));

    EXPECT_EQ(thread_option_count(), 2);
    EXPECT_EQ(thread_pool().thread_count(), 2U);

    thread_pool().shutdown();
    EXPECT_TRUE(execute("setoption name Threads value 3"));

    EXPECT_EQ(thread_option_count(), 2);
    EXPECT_EQ(thread_pool().thread_count(), 2U);
    EXPECT_NE(output.str().find("error: failed to resize thread pool"), std::string::npos)
        << output.str();
}

TEST_F(EngineOptionsTest, PonderOptionValuesAreCaseInsensitive) {
    EXPECT_TRUE(execute("setoption name pOnDeR value ON"));
    EXPECT_TRUE(ponder_enabled());

    EXPECT_TRUE(execute("setoption name PONDER value oFf"));
    EXPECT_FALSE(ponder_enabled());
}

TEST_F(EngineOptionsTest, HashOptionResizesAndClearHashClearsTT) {
    ASSERT_TRUE(execute("setoption name Hash value 8"));
    ASSERT_EQ(hash_option_mb(), 8);
    ASSERT_EQ(search::tt.capacity_mb(), 8U);

    search::tt.store(board().key(), Move(Square::E2, Square::E4), 42, 3, search::TTBound::Exact, 0);
    ASSERT_TRUE(search::tt.probe(board().key()).has_value());

    EXPECT_TRUE(execute("setoption name Clear Hash"));

    EXPECT_FALSE(search::tt.probe(board().key()).has_value());
}

struct SetOptionCase {
    std::string command;
    int         threads = uci::Options::default_threads;
    std::string output  = "error";
};

class SetOptionTest : public EngineOptionsTest,
                      public ::testing::WithParamInterface<SetOptionCase> {};

TEST_P(SetOptionTest, ValidateSetOption) {
    const auto& param = GetParam();

    // Execute the setoption command
    EXPECT_TRUE(execute(param.command));

    // Check if the thread count is set correctly and output is as expected
    EXPECT_EQ(thread_pool().thread_count(), param.threads);
    EXPECT_NE(output.str().find(param.output), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    SetOptionTests,
    SetOptionTest,
    ::testing::Values(
        SetOptionCase{.command = "setoption"},
        SetOptionCase{.command = "setoption abc"},
        SetOptionCase{.command = "setoption name"},
        SetOptionCase{.command = "setoption name abc"},
        SetOptionCase{.command = "setoption name Threads"},
        SetOptionCase{.command = "setoption name Threads abc"},
        SetOptionCase{.command = "setoption name Threads value"},
        SetOptionCase{.command = "setoption name Threads value abc"},
        SetOptionCase{.command = "setoption name Threads value 2 extra"},
        SetOptionCase{.command = "setoption name Threads value -1"},
        SetOptionCase{.command = "setoption name Threads value 0"},
        SetOptionCase{.command = "setoption name Threads value 99999"},
        SetOptionCase{.command = "setoption name Clear Hash value"},
        SetOptionCase{.command = "setoption name Threads value 4", .threads = 4, .output = ""}));
