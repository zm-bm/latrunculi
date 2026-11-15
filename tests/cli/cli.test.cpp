#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "cli/cli.hpp"

namespace cli {
namespace {

#if LATRUNCULI_TUNING
constexpr auto expected_usage = "Usage: latrunculi [bench|spsa]\n";
#else
constexpr auto expected_usage = "Usage: latrunculi [bench]\n";
#endif

void expect_top_level_usage(int argument_count, char* arguments[]) {
    std::istringstream input;
    std::ostringstream output;
    std::ostringstream diagnostics;

    EXPECT_EQ(run(argument_count, arguments, input, output, diagnostics), 1);
    EXPECT_TRUE(output.str().empty());
    EXPECT_EQ(diagnostics.str(), expected_usage);
}

TEST(CliTest, RunsUciWithoutArguments) {
    char executable[] = "latrunculi";
    char* arguments[]{executable};

    std::istringstream input{"isready\nquit\n"};
    std::ostringstream output;
    std::ostringstream diagnostics;

    EXPECT_EQ(run(1, arguments, input, output, diagnostics), 0);
    EXPECT_EQ(output.str(), "readyok\n");
    EXPECT_TRUE(diagnostics.str().empty());
}

TEST(CliTest, RejectsUnknownCommand) {
    char executable[] = "latrunculi";
    char command[] = "unknown";
    char* arguments[]{executable, command};

    expect_top_level_usage(2, arguments);
}

TEST(CliTest, RejectsBenchmarkArguments) {
    char executable[] = "latrunculi";
    char command[] = "bench";
    char extra[] = "extra";
    char* arguments[]{executable, command, extra};

    expect_top_level_usage(3, arguments);
}

} // namespace
} // namespace cli
