#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "cli/cli.hpp"

namespace cli {
namespace {

constexpr auto input_record = "game-1:12\t1-0\tk7/8/8/8/8/8/4r3/K2Q4 w - - 0 1\n";

void expect_top_level_usage(int argument_count, char* arguments[]) {
    std::istringstream input;
    std::ostringstream output;
    std::ostringstream diagnostics;

    EXPECT_EQ(run(argument_count, arguments, input, output, diagnostics), 1);
    EXPECT_TRUE(output.str().empty());
    EXPECT_EQ(diagnostics.str(), "Usage: latrunculi [bench|features [--settle]]\n");
}

TEST(CliTest, RunsUciWithoutArguments) {
    char  executable[] = "latrunculi";
    char* arguments[]{executable};

    std::istringstream input{"isready\nquit\n"};
    std::ostringstream output;
    std::ostringstream diagnostics;

    EXPECT_EQ(run(1, arguments, input, output, diagnostics), 0);
    EXPECT_EQ(output.str(), "readyok\n");
    EXPECT_TRUE(diagnostics.str().empty());
}

TEST(CliTest, ExportsRawFeatures) {
    char  executable[] = "latrunculi";
    char  command[]    = "features";
    char* arguments[]{executable, command};

    std::istringstream input(input_record);
    std::ostringstream output;
    std::ostringstream diagnostics;

    EXPECT_EQ(run(2, arguments, input, output, diagnostics), 0);
    EXPECT_TRUE(diagnostics.str().empty());
    EXPECT_NE(output.str().find(R"("fen":"k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1")"), std::string::npos);
}

TEST(CliTest, ExportsSettledFeatures) {
    char  executable[] = "latrunculi";
    char  command[]    = "features";
    char  option[]     = "--settle";
    char* arguments[]{executable, command, option};

    std::istringstream input(input_record);
    std::ostringstream output;
    std::ostringstream diagnostics;

    EXPECT_EQ(run(3, arguments, input, output, diagnostics), 0);
    EXPECT_TRUE(diagnostics.str().empty());
    EXPECT_NE(output.str().find(R"("fen":"k7/8/8/8/8/8/4Q3/K7 b - - 0 1")"), std::string::npos);
}

TEST(CliTest, RejectsInvalidFeatureArguments) {
    char  executable[] = "latrunculi";
    char  command[]    = "features";
    char  option[]     = "--unknown";
    char* arguments[]{executable, command, option};

    std::istringstream input;
    std::ostringstream output;
    std::ostringstream diagnostics;

    EXPECT_EQ(run(3, arguments, input, output, diagnostics), 1);
    EXPECT_TRUE(output.str().empty());
    EXPECT_EQ(diagnostics.str(), "Usage: latrunculi [bench|features [--settle]]\n");
}

TEST(CliTest, ReportsFeatureExportErrors) {
    char  executable[] = "latrunculi";
    char  command[]    = "features";
    char* arguments[]{executable, command};

    std::istringstream input{"missing-fields\n"};
    std::ostringstream output;
    std::ostringstream diagnostics;

    EXPECT_EQ(run(2, arguments, input, output, diagnostics), 1);
    EXPECT_EQ(diagnostics.str(), "features: invalid input at line 1\n");
}

TEST(CliTest, RejectsUnknownCommand) {
    char  executable[] = "latrunculi";
    char  command[]    = "unknown";
    char* arguments[]{executable, command};

    expect_top_level_usage(2, arguments);
}

TEST(CliTest, RejectsBenchmarkArguments) {
    char  executable[] = "latrunculi";
    char  command[]    = "bench";
    char  extra[]      = "extra";
    char* arguments[]{executable, command, extra};

    expect_top_level_usage(3, arguments);
}

} // namespace
} // namespace cli
