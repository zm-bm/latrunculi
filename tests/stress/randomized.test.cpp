#include <gtest/gtest.h>

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "board/board.hpp"
#include "board/notation.hpp"
#include "core/attacks.hpp"
#include "eval/evaluation.hpp"
#include "movegen/generator.hpp"
#include "search/limits.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"
#include "support/board_snapshot.hpp"
#include "support/search_reporter.hpp"

namespace {

constexpr std::uint64_t DEFAULT_SEED   = 0x52454C303031ULL;
constexpr std::size_t   DEFAULT_CASES  = 64;
constexpr int           MAX_PLIES      = 128;
constexpr int           SEARCH_DEPTH   = 3;
constexpr std::size_t   SEARCH_THREADS = 4;

struct StressConfig {
    std::uint64_t         seed       = DEFAULT_SEED;
    std::size_t           first_case = 0;
    std::size_t           case_count = DEFAULT_CASES;
    std::string           executable;
    std::filesystem::path working_directory;
};

StressConfig config;

class SplitMix64 {
public:
    explicit SplitMix64(std::uint64_t seed) : state(seed) {}

    std::uint64_t next() {
        std::uint64_t value = (state += 0x9E3779B97F4A7C15ULL);
        value               = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
        value               = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
        return value ^ (value >> 31);
    }

    std::size_t index(std::size_t size) {
        assert(size > 0);
        return static_cast<std::size_t>(next() % size);
    }

private:
    std::uint64_t state;
};

std::optional<std::uint64_t> parse_uint64(std::string_view text) {
    int base = 10;
    if (text.starts_with("0x") || text.starts_with("0X")) {
        text.remove_prefix(2);
        base = 16;
    }
    if (text.empty())
        return std::nullopt;

    std::uint64_t value     = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (error != std::errc{} || end != text.data() + text.size())
        return std::nullopt;
    return value;
}

std::optional<std::size_t> parse_size(std::string_view text) {
    const auto value = parse_uint64(text);
    if (!value || *value > std::numeric_limits<std::size_t>::max())
        return std::nullopt;
    return static_cast<std::size_t>(*value);
}

std::uint64_t case_seed(std::uint64_t seed, std::size_t case_index) {
    SplitMix64 generator(seed + (0x9E3779B97F4A7C15ULL * (case_index + 1)));
    return generator.next();
}

std::vector<Move> legal_moves(const Board& board) {
    std::vector<Move> moves;
    for (const Move move : movegen::generate_pseudo_legal(board)) {
        if (board.is_legal_pseudo_move(move))
            moves.push_back(move);
    }
    return moves;
}

std::string shell_quote(std::string_view text) {
    std::string quoted{"'"};
    for (const char character : text) {
        if (character == '\'')
            quoted += "'\\''";
        else
            quoted += character;
    }
    quoted += '\'';
    return quoted;
}

std::string format_uci_history(const std::vector<std::string>& moves) {
    if (moves.empty())
        return "(startpos)";

    std::ostringstream output;
    for (std::size_t index = 0; index < moves.size(); ++index) {
        if (index)
            output << ' ';
        output << moves[index];
    }
    return output.str();
}

std::string format_pgn(const std::vector<std::string>& moves) {
    std::ostringstream output;
    for (std::size_t index = 0; index < moves.size(); ++index) {
        if (index % 2 == 0)
            output << (index / 2) + 1 << ". ";
        output << moves[index] << ' ';
    }
    output << '*';
    return output.str();
}

std::string format_diagnostics(const RecordingSearchReporter& reporter) {
    if (reporter.diagnostics.empty())
        return "(none)";

    std::ostringstream output;
    for (const std::string& diagnostic : reporter.diagnostics)
        output << diagnostic << '\n';
    return output.str();
}

std::string replay_command(std::size_t case_index) {
    return std::format("cd {} && {} --seed 0x{:016x} --case {}",
                       shell_quote(config.working_directory.string()),
                       shell_quote(config.executable),
                       config.seed,
                       case_index);
}

std::string case_context(std::size_t case_index, std::uint64_t derived_seed) {
    return std::format("master seed: 0x{:016x}\n"
                       "case: {}\n"
                       "case seed: 0x{:016x}\n"
                       "reproduce: {}",
                       config.seed,
                       case_index,
                       derived_seed,
                       replay_command(case_index));
}

std::string position_context(int                             ply,
                             const Board&                    board,
                             const std::vector<std::string>& uci_history,
                             const std::vector<std::string>& san_history) {
    return std::format("ply: {}\nFEN: {}\nUCI history: {}\nPGN: {}",
                       ply,
                       board.to_fen(),
                       format_uci_history(uci_history),
                       format_pgn(san_history));
}

TEST(RandomizedStressTest, LegalPositionsRoundTripEvaluateAndSearch) {
    RecordingSearchReporter reporter;
    search::ThreadPool      pool{SEARCH_THREADS, reporter};
    search::tt.clear();

    for (std::size_t offset = 0; offset < config.case_count; ++offset) {
        const std::size_t   case_index   = config.first_case + offset;
        const std::uint64_t derived_seed = case_seed(config.seed, case_index);
        SplitMix64          random{derived_seed};
        const int           search_ply = 8 + static_cast<int>(random.next() % 40);
        bool                searched   = false;
        Board               board;

        std::vector<board_test::BoardSnapshot> positions;
        std::vector<std::string>               uci_history;
        std::vector<std::string>               san_history;
        positions.reserve(MAX_PLIES);
        uci_history.reserve(MAX_PLIES);
        san_history.reserve(MAX_PLIES);

        SCOPED_TRACE(case_context(case_index, derived_seed));

        for (int ply = 0; ply < MAX_PLIES; ++ply) {
            SCOPED_TRACE(position_context(ply, board, uci_history, san_history));

            EXPECT_EQ(board.key(), board.recompute_key());
            board_test::expect_base_terms_consistent(board);
            EXPECT_EQ(eval::evaluate(board), eval::evaluate_trace(board).value());

            const std::vector<Move> moves = legal_moves(board);
            if (moves.empty())
                break;

            const Move selected                         = moves[random.index(moves.size())];
            bool       selected_ends_searchable_playout = false;

            for (const Move move : moves) {
                SCOPED_TRACE(std::format("round-trip move: {}", move.str()));
                const auto before = board_test::snapshot_board(board);

                board.make(move);
                EXPECT_EQ(board.key(), board.recompute_key());
                board_test::expect_base_terms_consistent(board);
                EXPECT_EQ(eval::evaluate(board), eval::evaluate_trace(board).value());

                if (!searched && ply < search_ply && move == selected)
                    selected_ends_searchable_playout =
                        board.is_draw() || legal_moves(board).empty();

                board.unmake();
                board_test::expect_same_board_snapshot(board, before);
            }

            if (!searched && !board.is_draw()
                && (ply >= search_ply || selected_ends_searchable_playout)) {
                reporter.clear();

                search::Limits limits;
                limits.depth = SEARCH_DEPTH;
                ASSERT_TRUE(pool.start_search(board, limits));
                pool.wait();

                SCOPED_TRACE(std::format("search diagnostics:\n{}", format_diagnostics(reporter)));
                ASSERT_EQ(reporter.best_moves.size(), 1U);
                EXPECT_NE(std::find(moves.begin(), moves.end(), reporter.best_moves.front()),
                          moves.end());
                searched = true;
            }

            positions.push_back(board_test::snapshot_board(board));
            uci_history.push_back(selected.str());
            san_history.push_back(to_san(board, selected));
            board.make(selected);
        }

        {
            SCOPED_TRACE(position_context(
                static_cast<int>(uci_history.size()), board, uci_history, san_history));
            EXPECT_TRUE(searched) << "playout ended before its randomized search position";
        }

        while (!positions.empty()) {
            SCOPED_TRACE(std::format("unwind ply: {}\nFEN: {}\nUCI history: {}\nPGN: {}",
                                     positions.size(),
                                     board.to_fen(),
                                     format_uci_history(uci_history),
                                     format_pgn(san_history)));

            board.unmake();
            board_test::expect_same_board_snapshot(board, positions.back());
            positions.pop_back();
            uci_history.pop_back();
            san_history.pop_back();
        }

        EXPECT_EQ(board.to_fen(), Board::start_fen);
    }
}

int option_error(std::string_view message) {
    std::cerr << "latrunculi-stress: " << message << '\n'
              << "usage: latrunculi-stress [--seed N] [--cases N | --case N] "
                 "[GoogleTest options]\n";
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    config.executable        = argv[0];
    config.working_directory = std::filesystem::current_path();

    bool               selected_case  = false;
    bool               selected_count = false;
    std::vector<char*> gtest_arguments{argv[0]};
    gtest_arguments.reserve(static_cast<std::size_t>(argc) + 1);

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument != "--seed" && argument != "--case" && argument != "--cases") {
            gtest_arguments.push_back(argv[index]);
            continue;
        }
        if (++index >= argc)
            return option_error(std::format("missing value for {}", argument));

        const std::string_view value = argv[index];
        if (argument == "--seed") {
            const auto parsed = parse_uint64(value);
            if (!parsed)
                return option_error(std::format("invalid seed: {}", value));
            config.seed = *parsed;
        } else if (argument == "--case") {
            const auto parsed = parse_size(value);
            if (!parsed)
                return option_error(std::format("invalid case: {}", value));
            config.first_case = *parsed;
            config.case_count = 1;
            selected_case     = true;
        } else {
            const auto parsed = parse_size(value);
            if (!parsed || *parsed == 0)
                return option_error(std::format("invalid case count: {}", value));
            config.case_count = *parsed;
            selected_count    = true;
        }
    }

    if (selected_case && selected_count)
        return option_error("--case and --cases cannot be used together");
    if (config.case_count > std::numeric_limits<std::size_t>::max() - config.first_case)
        return option_error("case range overflows size_t");

    int gtest_argc = static_cast<int>(gtest_arguments.size());
    gtest_arguments.push_back(nullptr);
    ::testing::InitGoogleTest(&gtest_argc, gtest_arguments.data());
    attacks::init();
    return RUN_ALL_TESTS();
}
