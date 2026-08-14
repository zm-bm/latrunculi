#include "perft.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "board/board.hpp"
#include "movegen/perft.hpp"

namespace measurements {
namespace {

using MeasurementClock = std::chrono::steady_clock;

constexpr std::string_view result_format = "perft_measurement_v1";
constexpr std::string_view pos2_fen =
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
constexpr std::string_view pos3_fen = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
constexpr std::string_view pos4w_fen =
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
constexpr std::string_view pos4b_fen =
    "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1";
constexpr std::string_view pos5_fen = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
constexpr std::string_view pos6_fen =
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";

enum class OutputFormat { Text, Tsv };
enum class Profile { Smoke, Standard };

struct Options {
    OutputFormat format{OutputFormat::Text};
    Profile      profile{Profile::Smoke};
};

struct Case {
    std::string_view id;
    std::string_view fen;
    int              smoke_depth;
    NodeCount        smoke_nodes;
    int              standard_depth;
    NodeCount        standard_nodes;
};

struct Row {
    std::string   case_id;
    std::string   profile;
    int           depth{0};
    NodeCount     nodes{0};
    NodeCount     expected_nodes{0};
    std::uint64_t total_ns{0};
    double        nodes_per_second{0.0};
};

std::string to_string(Profile profile) {
    return profile == Profile::Smoke ? "smoke" : "standard";
}

std::vector<Case> make_cases(Profile profile) {
    std::vector<Case> cases = {
        {"startpos", Board::start_fen, 4, 197281, 4, 197281},
        {"pos3", pos3_fen, 4, 43238, 4, 43238},
        {"pos5", pos5_fen, 3, 62379, 4, 2103487},
    };

    if (profile == Profile::Standard) {
        cases.push_back({"pos2", pos2_fen, 3, 97862, 4, 4085603});
        cases.push_back({"pos4w", pos4w_fen, 3, 9467, 4, 422333});
        cases.push_back({"pos4b", pos4b_fen, 3, 9467, 4, 422333});
        cases.push_back({"pos6", pos6_fen, 3, 89890, 4, 3894594});
    }

    return cases;
}

Row measure(const Case& perft_case, Profile profile) {
    Board      board(perft_case.fen);
    const auto initial_key = board.key();
    const int  depth =
        profile == Profile::Smoke ? perft_case.smoke_depth : perft_case.standard_depth;
    const NodeCount expected =
        profile == Profile::Smoke ? perft_case.smoke_nodes : perft_case.standard_nodes;
    NodeCount nodes = 0;

    const auto start    = MeasurementClock::now();
    nodes               = movegen::perft(board, depth);
    const auto end      = MeasurementClock::now();
    const auto total_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

    if (nodes != expected) {
        std::ostringstream message;
        message << "perft mismatch for " << perft_case.id << " depth " << depth << ": got " << nodes
                << ", expected " << expected;
        throw std::runtime_error(message.str());
    }
    if (board.key() != initial_key || board.recompute_key() != initial_key)
        throw std::runtime_error("board restoration failed for " + std::string(perft_case.id));

    const double seconds = static_cast<double>(total_ns) / 1'000'000'000.0;
    return {
        .case_id          = std::string(perft_case.id),
        .profile          = to_string(profile),
        .depth            = depth,
        .nodes            = nodes,
        .expected_nodes   = expected,
        .total_ns         = total_ns,
        .nodes_per_second = seconds > 0.0 ? static_cast<double>(nodes) / seconds : 0.0,
    };
}

void emit_tsv(const std::vector<Row>& rows) {
    std::cout << "result_format\tcase\tprofile\tdepth\tnodes\texpected_nodes\ttotal_ns\t"
                 "nodes_per_second\n";
    for (const Row& row : rows) {
        std::cout << result_format << '\t' << row.case_id << '\t' << row.profile << '\t'
                  << row.depth << '\t' << row.nodes << '\t' << row.expected_nodes << '\t'
                  << row.total_ns << '\t' << std::fixed << std::setprecision(3)
                  << row.nodes_per_second << '\n';
    }
}

void emit_text(const std::vector<Row>& rows) {
    for (const Row& row : rows) {
        const double total_ms = static_cast<double>(row.total_ns) / 1'000'000.0;
        std::cout << row.case_id << " depth " << row.depth << ": " << row.nodes << " nodes in "
                  << std::fixed << std::setprecision(3) << total_ms << " ms ("
                  << std::setprecision(0) << row.nodes_per_second << " nps)\n";
    }
}

Profile parse_profile(std::string_view value) {
    if (value == "smoke")
        return Profile::Smoke;
    if (value == "standard")
        return Profile::Standard;
    throw std::runtime_error("unknown profile: " + std::string(value));
}

OutputFormat parse_format(std::string_view value) {
    if (value == "text")
        return OutputFormat::Text;
    if (value == "tsv")
        return OutputFormat::Tsv;
    throw std::runtime_error("unknown format: " + std::string(value));
}

void print_usage(const char* argv0) {
    std::cerr << "Perft measurement for recursive move generation validation.\n";
    std::cerr << "Usage: " << argv0 << " [--profile smoke|standard] [--format text|tsv]\n";
}

Options parse_args(int argc, char* argv[]) {
    Options options;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];

        if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }

        if (argument == "--profile") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --profile");
            options.profile = parse_profile(argv[index]);
            continue;
        }

        if (argument == "--format") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --format");
            options.format = parse_format(argv[index]);
            continue;
        }

        throw std::runtime_error("unknown argument: " + std::string(argument));
    }

    return options;
}

} // namespace

int run_perft(int argc, char* argv[]) {
    try {
        const Options    options = parse_args(argc, argv);
        std::vector<Row> rows;
        for (const Case& perft_case : make_cases(options.profile))
            rows.push_back(measure(perft_case, options.profile));

        if (options.format == OutputFormat::Tsv)
            emit_tsv(rows);
        else
            emit_text(rows);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}

} // namespace measurements
