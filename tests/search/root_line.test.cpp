#include <array>
#include <string_view>

#include <gtest/gtest.h>

#include "search/root_line.hpp"

namespace search {

namespace {

RootLine completed_root_line(Move move, EvalValue value, int depth) {
    return RootLine{.root_move = move, .value = value, .depth = depth, .completed = true};
}

} // namespace

TEST(RootLineTest, SelectsByDepthValueAndMoveBits) {
    struct Case {
        std::string_view        name;
        RootLine                fallback;
        std::array<RootLine, 2> candidates;
        Move                    expected_move;
    };

    const std::array cases{
        Case{
            .name          = "greater depth",
            .fallback      = completed_root_line(Move(E2, E4), 50, 2),
            .candidates    = {completed_root_line(Move(G1, F3), 0, 3),
                              completed_root_line(Move(D2, D4), 200, 1)},
            .expected_move = Move(G1, F3),
        },
        Case{
            .name          = "greater value at equal depth",
            .fallback      = completed_root_line(Move(E2, E4), 10, 3),
            .candidates    = {completed_root_line(Move(G1, F3), 25, 3),
                              completed_root_line(Move(D2, D4), 20, 3)},
            .expected_move = Move(G1, F3),
        },
        Case{
            .name          = "lower move bits at equal depth and value",
            .fallback      = completed_root_line(Move(H2, H3), 10, 3),
            .candidates    = {completed_root_line(Move(G2, G3), 10, 3),
                              completed_root_line(Move(A2, A3), 10, 3)},
            .expected_move = Move(A2, A3),
        },
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        const RootLine selected = select_best_root_line(tc.fallback, tc.candidates);
        EXPECT_EQ(selected.root_move, tc.expected_move);
    }
}

TEST(RootLineTest, PreservesFallbackWhenCandidatesAreUnusable) {
    const std::array candidates{
        RootLine{.root_move = Move(G1, F3), .value = 1000, .depth = 99, .completed = false},
        RootLine{.root_move = NULL_MOVE, .value = 1000, .depth = 99, .completed = true},
        RootLine{.root_move = Move(D2, D4), .value = 1000, .depth = 0, .completed = true},
    };

    const RootLine usable_fallback = completed_root_line(Move(E2, E4), 10, 2);
    EXPECT_EQ(select_best_root_line(usable_fallback, candidates), usable_fallback);

    const RootLine unusable_fallback{
        .root_move = NULL_MOVE, .value = eval_value::draw, .depth = 1, .completed = true};
    EXPECT_EQ(select_best_root_line(unusable_fallback, candidates), unusable_fallback);
}

} // namespace search
