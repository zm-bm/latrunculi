#include <gtest/gtest.h>

#include <array>
#include <sstream>
#include <thread>
#include <vector>

#include "board/board.hpp"
#include "cli/cli.hpp"
#include "eval/evaluation.hpp"
#include "eval/features.hpp"
#include "eval/nnue.hpp"
#include "movegen/moves.hpp"
#include "nnue_embedded.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"
#include "support/search_reporter.hpp"
#include "support/search_test_access.hpp"
#include "support/search_thread_test_access.hpp"

TEST(NeuralTest, OrientationAndColorReflection) {
    EXPECT_EQ(nnue::feature(WHITE, WHITE, PAWN, A2), 8);
    EXPECT_EQ(nnue::feature(BLACK, BLACK, PAWN, A7), 8);
    EXPECT_EQ(nnue::feature(BLACK, WHITE, KING, H1), 11 * 64 + 63);
    const Board a("3k4/8/1b6/8/5P2/1N6/8/4K3 w - - 0 1");
    const Board b("4k3/8/1n6/5p2/8/1B6/8/3K4 b - - 0 1");
    EXPECT_EQ(eval::evaluate(a), eval::evaluate(b));
}

TEST(NeuralTest, NullMovesAndReloadsRestoreExactAccumulators) {
    Board board;
    const auto original = board.neural_state();
    board.make_null();
    EXPECT_EQ(board.neural_state(), original);
    EXPECT_EQ(eval::evaluate(board), nnue::evaluate(original, BLACK));
    board.unmake_null();
    EXPECT_EQ(board.neural_state(), original);
    const Board copy(board);
    board.load_fen("4k3/P7/8/8/8/8/7p/4K3 b - - 0 1");
    EXPECT_EQ(board.neural_state(), nnue::refresh(board));
    board = copy;
    EXPECT_EQ(board.neural_state(), original);
    board.load_fen(Board::start_fen);
    EXPECT_EQ(board.neural_state(), original);
}

TEST(NeuralTest, EverySpecialMoveRoundTrips) {
    for (const char* fen : {"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
                           "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
                           "1r2k3/P7/8/8/8/8/7p/4K3 w - - 0 1"}) {
        Board board(fen);
        const auto original = board.neural_state();
        movegen::UnscoredMoves moves(board);
        for (Move m = moves.next(); !m.is_null(); m = moves.next()) {
            board.make(m);
            EXPECT_EQ(board.neural_state(), nnue::refresh(board)) << m.str();
            Board copy(board);
            EXPECT_EQ(copy.neural_state(), board.neural_state());
            board.unmake();
            EXPECT_EQ(board.neural_state(), original);
        }
    }
}

TEST(NeuralTest, MutableBoardStateIsIndependentAcrossThreads) {
    const Board root;
    std::array<bool, 2> correct{};
    std::array<std::thread, 2> threads;
    for (int i = 0; i < 2; ++i)
        threads[i] = std::thread([&, i] {
            Board board(root);
            movegen::UnscoredMoves moves(board);
            for (int j = 0; j <= i; ++j) {
                const auto move = moves.next();
                board.make(move);
                const bool equal = board.neural_state() == nnue::refresh(board);
                board.unmake();
                if (!equal) return;
            }
            correct[i] = board.neural_state() == root.neural_state();
        });
    for (auto& t : threads) t.join();
    EXPECT_TRUE(correct[0]);
    EXPECT_TRUE(correct[1]);
    EXPECT_EQ(root.neural_state(), nnue::refresh(root));
}

TEST(NeuralTest, HceFeatureExportsFailExplicitly) {
    const Board board;
    EXPECT_THROW((void)eval::extract_features(board), std::logic_error);
    std::istringstream input;
    std::ostringstream output;
    EXPECT_THROW(eval::export_features(input, output), std::logic_error);
    EXPECT_TRUE(output.str().empty());
}

TEST(NeuralTest, WorkersRestoreNeuralStateAcrossSearchStopAndRootReset) {
    RecordingSearchReporter reporter;
    search::ThreadPool pool(2, reporter);
    search::tt.resize(1);
    Board root;
    constexpr std::array positions = {Board::start_fen,
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
        "1r2k3/P7/8/8/8/8/7p/4K3 w - - 0 1"};
    for (std::size_t round = 0; round < positions.size(); ++round) {
        root.load_fen(positions[round]);
        const auto original = root.neural_state();
        search::Limits limits;
        limits.set_depth(3);
        limits.set_nodes(10000);
        ASSERT_TRUE(pool.start_search(root, limits));
        if (round == 1) pool.request_stop();
        pool.wait();
        EXPECT_EQ(root.neural_state(), original);
        for (std::size_t i = 0; i < 2; ++i) {
            auto& worker = SearchThreadTestAccess::worker(pool, i);
            const auto& board = SearchTestAccess::board(worker);
            EXPECT_EQ(board.to_fen(), root.to_fen());
            EXPECT_EQ(board.neural_state(), original);
            EXPECT_EQ(board.neural_state(), nnue::refresh(board));
        }
        pool.clear_search_heuristics();
        search::tt.clear();
    }
}

TEST(NeuralTest, SignedDivisionClippingOrderingAndScoreClamps) {
    nnue::Network weights{};
    weights.calibration = 32;
    nnue::Accumulators state;
    weights.output_bias = -(255 * 4096) + 1;
    EXPECT_EQ(nnue::evaluate(weights, state, WHITE), -31);
    weights.output_bias = (255 * 4096) - 1;
    EXPECT_EQ(nnue::evaluate(weights, state, WHITE), 31);
    weights.output_bias = 0;
    weights.output[0] = 4096;
    state.values[WHITE][0] = 255;
    state.values[BLACK][0] = -255;
    EXPECT_EQ(nnue::evaluate(weights, state, WHITE), 32);
    EXPECT_EQ(nnue::evaluate(weights, state, BLACK), 0);
    state.values[WHITE][0] = 1000000;
    EXPECT_EQ(nnue::evaluate(weights, state, WHITE), 32);
    weights.output_bias = 1000000000000LL;
    EXPECT_EQ(nnue::evaluate(weights, state, WHITE), nnue::static_limit);
    weights.output_bias = -1000000000000LL;
    EXPECT_EQ(nnue::evaluate(weights, state, WHITE), -nnue::static_limit);
}

TEST(NeuralTest, NetworkImportRejectsCorruptionAndOverflow) {
    std::vector<std::uint8_t> data(nnue::embedded::bytes.begin(), nnue::embedded::bytes.end());
    EXPECT_NO_THROW(nnue::decode(data));
    for (std::size_t offset : {0U, 8U, 12U, 20U, 28U, 36U, 40U, 44U, 52U}) {
        data[offset] ^= 1;
        EXPECT_THROW(nnue::decode(data), std::invalid_argument) << offset;
        data[offset] ^= 1;
    }
    data.pop_back();
    EXPECT_THROW(nnue::decode(data), std::invalid_argument);
    data.assign(nnue::embedded::bytes.begin(), nnue::embedded::bytes.end());
    // A checksum-valid INT64_MIN output bias must fail the arithmetic proof.
    std::fill(data.end() - 8, data.end(), 0);
    data.back() = 128;
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::size_t i = 0; i < data.size(); ++i)
        if (i < 44 || i >= 52) hash = (hash ^ data[i]) * 1099511628211ULL;
    for (int i = 0; i < 8; ++i) data[44 + i] = std::uint8_t(hash >> (8 * i));
    EXPECT_THROW(nnue::decode(data), std::invalid_argument);
}
