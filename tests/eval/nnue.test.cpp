#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <sstream>
#include <thread>
#include <vector>

#include "board/board.hpp"
#include "eval/evaluation.hpp"
#include "eval/features.hpp"
#include "eval/nnue.hpp"
#include "eval/nnue_kernels.hpp"
#include "movegen/moves.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"
#include "support/board_snapshot.hpp"
#include "support/search_reporter.hpp"
#include "support/search_test_access.hpp"
#include "support/search_thread_test_access.hpp"

TEST(HalfKpTest, LegacyFeatureCoordinatesAndColorEncoding) {
    EXPECT_EQ(nnue::feature(WHITE, E1, WHITE, PAWN, A2), 2573);
    EXPECT_EQ(nnue::feature(BLACK, E8, BLACK, PAWN, A7), 1939);
    EXPECT_EQ(nnue::feature(WHITE, E1, BLACK, PAWN, A7), 2677);
    EXPECT_EQ(nnue::network().weights.size(), 41024 * 256);
}

TEST(HalfKpTest, BackendSelectionSafelyHandlesUnsupportedCpus) {
    EXPECT_EQ(nnue::resolve_backend(nnue::BackendPreference::Auto, false), nnue::Backend::Scalar);
    EXPECT_EQ(nnue::resolve_backend(nnue::BackendPreference::Scalar, true), nnue::Backend::Scalar);
    EXPECT_EQ(nnue::resolve_backend(nnue::BackendPreference::Auto, true), nnue::Backend::Avx2);
    EXPECT_EQ(nnue::resolve_backend(nnue::BackendPreference::Avx2, true), nnue::Backend::Avx2);
    EXPECT_THROW(nnue::resolve_backend(nnue::BackendPreference::Avx2, false), std::runtime_error);
}

TEST(HalfKpTest, FrozenLoaderRejectsIncompatibleCorruptAndTruncatedFiles) {
    const auto embedded = nnue::embedded_network();
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

TEST(HalfKpTest, AdapterUsesSignedTruncationAndStaysOutsideMateBand) {
    EXPECT_EQ(nnue::adapt(0), 27);
    EXPECT_EQ(nnue::adapt(-1), 26);
    EXPECT_EQ(nnue::adapt(-23), 0);
    EXPECT_EQ(nnue::adapt(-24), -1);
    EXPECT_EQ(nnue::adapt(24), 56);
    EXPECT_EQ(nnue::adapt(std::numeric_limits<int>::max()), 30000);
    EXPECT_EQ(nnue::adapt(std::numeric_limits<int>::min()), -30000);
    EXPECT_LT(nnue::static_limit, scores::tt_mate_bound);
}

TEST(HalfKpTest, SpecialMovesCopiesAndFailedFenRestoreExactState) {
    for (const char* fen :
         {Board::start_fen,
          "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
          "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
          "4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1",
          "1r2k3/P7/8/8/8/8/7p/4K3 w - - 0 1",
          "4k3/8/8/8/8/8/p7/1R2K3 b - - 0 1"}) {
        Board board(fen);
        const auto before = board_test::snapshot_board(board);
        movegen::UnscoredMoves moves(board);
        for (Move move = moves.next(); !move.is_null(); move = moves.next()) {
            board.make(move);
            EXPECT_EQ(board.neural_state(), nnue::refresh(board, nnue::Backend::Scalar))
                << move.str();
            Board copy(board), assigned;
            assigned = board;
            EXPECT_EQ(copy.neural_state(), board.neural_state());
            EXPECT_EQ(assigned.neural_state(), board.neural_state());
            EXPECT_THROW(board.load_fen("invalid"), std::invalid_argument);
            EXPECT_EQ(board.neural_state(), copy.neural_state());
            board.unmake();
            board_test::expect_same_board_snapshot(board, before);
        }
        if (!board.is_check()) {
            board.make_null();
            EXPECT_EQ(board.neural_state(), before.neural_state);
            EXPECT_EQ(eval::evaluate(board),
                      nnue::evaluate(before.neural_state, board.side_to_move()));
            board.unmake_null();
            board_test::expect_same_board_snapshot(board, before);
        }
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&board.neural_state()) % 64, 0);
    }
}

TEST(HalfKpTest, HceExportsFailBeforeEmittingSchema) {
    Board board;
    EXPECT_THROW((void)eval::extract_features(board), std::logic_error);
    std::istringstream input;
    std::ostringstream output;
    EXPECT_THROW(eval::export_features(input, output), std::logic_error);
    EXPECT_TRUE(output.str().empty());
}

TEST(HalfKpTest, Avx2MatchesEveryIntermediateAtClippingBoundaries) {
    if (!nnue::avx2_available())
        GTEST_SKIP() << "AVX2 unavailable";
    nnue::Accumulators state;
    constexpr std::array<std::int16_t, 12> values =
        {-32768, -8192, -129, -1, 0, 1, 63, 64, 126, 127, 128, 32767};
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 256; ++i)
            state.values[c][i] = values[(i + c) % values.size()];
    for (Color turn : {BLACK, WHITE})
        EXPECT_EQ(nnue::trace(nnue::network(), state, turn, nnue::Backend::Scalar),
                  nnue::trace(nnue::network(), state, turn, nnue::Backend::Avx2));
    std::array<std::uint8_t, 512> input;
    input.fill(127);
    std::array<std::int8_t, 512 * 32> weights;
    std::array<std::int32_t, 32> bias{}, scalar{}, avx{};
    for (int weight : {-128, 127}) {
        weights.fill(std::int8_t(weight));
        nnue::kernels::dense_scalar(
            input.data(),
            weights.data(),
            bias.data(),
            scalar.data(),
            512,
            32);
        nnue::kernels::dense_avx2(input.data(), weights.data(), bias.data(), avx.data(), 512, 32);
        EXPECT_EQ(scalar, avx);
    }
    constexpr std::array<std::int32_t, 10> clips = {
        std::numeric_limits<std::int32_t>::min(),
        -65,
        -1,
        0,
        63,
        64,
        8127,
        8128,
        8192,
        std::numeric_limits<std::int32_t>::max()};
    for (int i = 0; i < 32; ++i)
        scalar[i] = clips[i % clips.size()];
    std::array<std::uint8_t, 32> a{}, b{};
    nnue::kernels::activate_scalar(scalar.data(), a.data());
    nnue::kernels::activate_avx2(scalar.data(), b.data());
    EXPECT_EQ(a, b);
}

TEST(HalfKpTest, WorkerStateRemainsIndependentAcrossStopResetAndRepeatedSearches) {
    RecordingSearchReporter reporter;
    search::ThreadPool pool(2, reporter);
    search::tt.resize(1);
    Board root;
    constexpr std::array positions = {
        Board::start_fen,
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
        "1r2k3/P7/8/8/8/8/7p/4K3 w - - 0 1"};
    for (int round = 0; round < 3; ++round) {
        root.load_fen(positions[round]);
        const auto before = board_test::snapshot_board(root);
        search::Limits limits;
        limits.set_depth(3);
        limits.set_nodes(10000);
        ASSERT_TRUE(pool.start_search(root, limits));
        if (round == 1)
            pool.request_stop();
        pool.wait();
        board_test::expect_same_board_snapshot(root, before);
        for (int i = 0; i < 2; ++i) {
            auto& worker = SearchThreadTestAccess::worker(pool, i);
            const auto& board = SearchTestAccess::board(worker);
            EXPECT_EQ(board.to_fen(), root.to_fen());
            EXPECT_EQ(board.neural_state(), root.neural_state());
            EXPECT_EQ(board.neural_state(), nnue::refresh(board));
        }
        pool.clear_search_heuristics();
        search::tt.clear();
    }
}
