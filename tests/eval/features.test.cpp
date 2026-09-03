#include "eval/features.hpp"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "eval/evaluation.hpp"
#include "eval/parameters.hpp"
#include "support/board_fixtures.hpp"

namespace {

std::uint64_t schema_name_hash(const auto& schema) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto& feature : schema) {
        for (const unsigned char ch : feature.name) {
            hash ^= ch;
            hash *= 1099511628211ull;
        }
        hash ^= 0;
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace

TEST(FeaturesTest, SchemaIsStableAndUnique) {
    const auto& schema = eval::feature_schema();

    EXPECT_EQ(eval::feature_schema_version, 1);
    EXPECT_EQ(schema.size(), 482u);
    EXPECT_EQ(schema.front().name, "material.pawn");
    EXPECT_EQ(schema.back().name, "threat.weak_queen");
    // Feature names and IDs must not change without a schema-version bump.
    EXPECT_EQ(schema_name_hash(schema), 6951263749403373319ull);

    std::unordered_set<std::string> names;
    for (const auto& feature : schema) {
        EXPECT_FALSE(feature.name.empty());
        EXPECT_TRUE(names.insert(feature.name).second) << feature.name;
    }
}

TEST(FeaturesTest, RawCoefficientsReconstructProductionEvaluation) {
    const std::vector<std::string> fens = {
        board_test::fen::start,
        board_test::fen::kings_only,
        board_test::fen::white_pawn_e2,
        "r3k2r/pp1n1ppp/2p1p3/3pP3/3P1P2/2N3P1/PP3P1P/R3K2R w KQkq - 2 14",
        "4k3/5n2/8/8/8/8/4P3/4K1NR w - - 0 2",
        "8/4k3/2p5/3P4/8/4K3/8/8 b - - 17 54",
    };

    for (const std::string& fen : fens) {
        const Board               board(fen);
        const eval::FeatureRecord record = eval::extract_features(board);

        EXPECT_EQ(record.fixed_score, record.term(eval::Term::KingSafety).total()) << fen;
        EXPECT_EQ(record.reconstruct(), record.value) << fen;
        EXPECT_EQ(record.value, eval::evaluate(board)) << fen;
    }
}

TEST(FeaturesTest, CoefficientsUseWhiteRelativePerspective) {
    const Board               board(board_test::fen::white_pawn_e2);
    const eval::FeatureRecord record = eval::extract_features(board);

    EXPECT_EQ(record.coefficients[eval::feature::material(PAWN)], 1);
    EXPECT_EQ(record.coefficients[eval::feature::piece_square(PAWN, E2)], 1);
    EXPECT_EQ(record.phase_counts, (std::array<int, 4>{0, 0, 0, 0}));
    EXPECT_EQ(record.pawn_counts[WHITE], 1);
    EXPECT_EQ(record.pawn_counts[BLACK], 0);
    EXPECT_EQ(record.turn, WHITE);
}

TEST(FeaturesTest, EvaluationIsColorSymmetric) {
    const Board original("6k1/8/8/4pNp1/3nP1P1/2P5/8/6K1 w - - 0 1");
    const Board color_swapped("1k6/8/5p2/1p1pN3/1PnP4/8/8/1K6 b - - 0 1");

    const eval::FeatureRecord original_record = eval::extract_features(original);
    const eval::FeatureRecord swapped_record  = eval::extract_features(color_swapped);

    ASSERT_NE(original_record.coefficients[eval::feature::material(PAWN)], 0);
    for (std::size_t id = 0; id < original_record.coefficients.size(); ++id) {
        SCOPED_TRACE(id);
        EXPECT_EQ(original_record.coefficients[id], -swapped_record.coefficients[id]);
    }

    EXPECT_EQ(original_record.phase_counts, swapped_record.phase_counts);
    ASSERT_NE(original_record.pawn_counts[WHITE], original_record.pawn_counts[BLACK]);
    EXPECT_EQ(original_record.pawn_counts[WHITE], swapped_record.pawn_counts[BLACK]);
    EXPECT_EQ(original_record.pawn_counts[BLACK], swapped_record.pawn_counts[WHITE]);

    for (int index = 0; index < static_cast<int>(eval::Term::Count); ++index) {
        SCOPED_TRACE(index);
        const auto             term          = static_cast<eval::Term>(index);
        const eval::TermScore& original_term = original_record.term(term);
        const eval::TermScore& swapped_term  = swapped_record.term(term);

        EXPECT_EQ(original_term.total(), -swapped_term.total());
        if (term != eval::Term::Material && term != eval::Term::PieceSquare) {
            EXPECT_EQ(original_term.white, swapped_term.black);
            EXPECT_EQ(original_term.black, swapped_term.white);
        } else {
            EXPECT_EQ(original_term.white, -swapped_term.white);
            EXPECT_EQ(original_term.black, eval::TaperedScore::Zero);
            EXPECT_EQ(swapped_term.black, eval::TaperedScore::Zero);
        }
    }

    EXPECT_EQ(original_record.unscaled_score, -swapped_record.unscaled_score);
    EXPECT_EQ(original_record.scaled_score, -swapped_record.scaled_score);
    EXPECT_EQ(original_record.tapered_value, -swapped_record.tapered_value);
    EXPECT_EQ(original_record.side_to_move_value, swapped_record.side_to_move_value);
    EXPECT_EQ(original_record.value, swapped_record.value);
    EXPECT_EQ(original_record.white_value(), -swapped_record.white_value());
    EXPECT_EQ(original_record.turn, ~swapped_record.turn);
}

TEST(FeaturesTest, RecordMatchesNormalEvaluationAndFormatsStableTermBreakdown) {
    const Board               board(board_test::fen::start);
    const eval::FeatureRecord record = eval::extract_features(board);
    const std::string         output = eval::format_evaluation(record);

    EXPECT_EQ(record.value, eval::evaluate(board));
    EXPECT_EQ(record.term_total(), record.unscaled_score);
    EXPECT_EQ(record.side_to_move_value + eval::tempo_bonus, record.value);

    EXPECT_NE(output.find("Term"), std::string::npos);
    EXPECT_NE(output.find("Material"), std::string::npos);
    EXPECT_NE(output.find("Piece Sq."), std::string::npos);
    EXPECT_NE(output.find("Mobility"), std::string::npos);
    EXPECT_NE(output.find("Evaluation:"), std::string::npos);
}

TEST(FeaturesTest, BatchExportIsDeterministic) {
    const std::string input = "game-1:12\t1-0\t4k3/8/8/8/8/8/4P3/4K3 w - - 0 1\n"
                              "game-2:31\t1/2-1/2\t8/4k3/2p5/3P4/8/4K3/8/8 b - - 17 54\n"
                              "game-3:8\t0-1\t4k3/4p3/8/8/8/8/8/4K3 b - - 0 1\n";

    const auto run = [&] {
        std::istringstream in(input);
        std::ostringstream out;
        eval::export_features(in, out);
        return out.str();
    };

    const std::string first = run();
    EXPECT_EQ(first, run());
    EXPECT_EQ(std::count(first.begin(), first.end(), '\n'), 4);
    EXPECT_NE(first.find(R"("type":"schema","version":1)"), std::string::npos);
    EXPECT_NE(
        first.find(
            R"("perspective":{"coefficients":"white","fixed":"white","eval":"side_to_move"})"),
        std::string::npos);
    EXPECT_NE(first.find(R"("source":"game-1:12","result":1)"), std::string::npos);
    EXPECT_NE(first.find(R"("source":"game-2:31","result":0)"), std::string::npos);
    EXPECT_NE(first.find(R"("source":"game-3:8","result":-1)"), std::string::npos);

    const std::string_view records = std::string_view(first).substr(first.find('\n') + 1);
    EXPECT_NE(records.find(R"("coefficients":[)"), std::string::npos);
    EXPECT_EQ(records.find(R"("features":[)"), std::string::npos);
}

TEST(FeaturesTest, BatchExportPreparesOrSkipsPositions) {
    std::istringstream input("game-1:12\t1-0\t4k3/8/8/8/8/8/4P3/4K3 w - - 0 1\n"
                             "game-2:18\t0-1\t4k3/8/8/8/8/8/3P4/4K3 w - - 0 1\n");
    std::ostringstream output;
    int                calls = 0;

    eval::export_features(input, output, [&](Board& board) {
        ++calls;
        if (calls == 2)
            return false;
        const Board prepared{"4k3/8/8/8/8/4P3/8/4K3 b - - 0 1"};
        board = prepared;
        return true;
    });
    EXPECT_EQ(calls, 2);
    const std::string text = output.str();
    EXPECT_NE(text.find(R"("fen":"4k3/8/8/8/8/4P3/8/4K3 b - - 0 1")"), std::string::npos);
    EXPECT_EQ(text.find(R"("source":"game-2:18")"), std::string::npos);
    EXPECT_EQ(std::count(text.begin(), text.end(), '\n'), 2);
}

TEST(FeaturesTest, BatchExportEscapesJsonStrings) {
    std::string source = "quote:\" slash:\\ backspace:\b formfeed:\f carriage:\r unit:";
    source.push_back('\x1f');

    std::istringstream input(source + "\t1-0\t4k3/8/8/8/8/8/4P3/4K3 w - - 0 1\n");
    std::ostringstream output;

    eval::export_features(input, output);
    EXPECT_NE(
        output.str().find(
            R"("source":"quote:\" slash:\\ backspace:\b formfeed:\f carriage:\r unit:\u001f")"),
        std::string::npos);
}

TEST(FeaturesTest, BatchExportRejectsInvalidInput) {
    std::istringstream input("missing-fields\n");
    std::ostringstream output;

    try {
        eval::export_features(input, output);
        FAIL() << "expected malformed input to fail";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "invalid input at line 1");
    }
}

TEST(FeaturesTest, BatchExportRejectsInvalidFen) {
    std::istringstream input("game-1\t1-0\tnot-a-fen\n");
    std::ostringstream output;

    try {
        eval::export_features(input, output);
        FAIL() << "expected invalid FEN to fail";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "invalid FEN at line 1: invalid fen, must have 4 or 6 fields");
    }
}
