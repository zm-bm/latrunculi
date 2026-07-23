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
#include "board/ply_state.hpp"
#include "core/attacks.hpp"
#include "movegen/movegen.hpp"

namespace {

using BenchClock = std::chrono::steady_clock;

constexpr const char* STARTFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr const char* POS2 = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
constexpr const char* POS3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
constexpr const char* POS4W = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
constexpr const char* POS4B = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1";
constexpr const char* POS5  = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
constexpr const char* POS6 =
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";

enum class OutputFormat { Text, Tsv };
enum class Profile { Smoke, Standard };

struct Options {
    OutputFormat format{OutputFormat::Text};
    Profile      profile{Profile::Smoke};
};

struct PerftCase {
    std::string_view id;
    std::string_view fen;
    int              smoke_depth;
    NodeCount        smoke_nodes;
    int              standard_depth;
    NodeCount        standard_nodes;
};

struct PerftRow {
    std::string   case_id;
    std::string   profile;
    int           depth{0};
    NodeCount     nodes{0};
    NodeCount     expected_nodes{0};
    std::uint64_t total_ns{0};
    double        nodes_per_sec{0.0};
};

template <typename Fn>
std::uint64_t measure_ns(Fn&& fn) {
    const auto start = BenchClock::now();
    fn();
    const auto end = BenchClock::now();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
}

NodeCount
perft_nodes(Board& board, int depth, PlyStateStack& states, int ply, PlyState& current_state) {
    if (depth == 0)
        return 1;

    NodeCount nodes    = 0;
    auto      movelist = movegen::generate_pseudo_legal(board);

    for (auto& move : movelist) {
        if (!board.is_legal_generated_move(move))
            continue;

        board.make(move, states.child(ply));
        nodes += perft_nodes(board, depth - 1, states, ply + 1, states.child(ply));
        board.unmake(current_state);
    }

    return nodes;
}

std::string to_string(Profile profile) {
    return profile == Profile::Smoke ? "smoke" : "standard";
}

std::vector<PerftCase> perft_cases(Profile profile) {
    std::vector<PerftCase> cases = {
        {"startpos", STARTFEN, 4, 197281, 4, 197281},
        {"pos3", POS3, 4, 43238, 4, 43238},
        {"pos5", POS5, 3, 62379, 4, 2103487},
    };

    if (profile == Profile::Standard) {
        cases.push_back({"pos2", POS2, 3, 97862, 4, 4085603});
        cases.push_back({"pos4w", POS4W, 3, 9467, 4, 422333});
        cases.push_back({"pos4b", POS4B, 3, 9467, 4, 422333});
        cases.push_back({"pos6", POS6, 3, 89890, 4, 3894594});
    }

    return cases;
}

PerftRow run_perft_case(const PerftCase& perft_case, Profile profile) {
    PlyState   root;
    Board      board(root, std::string(perft_case.fen));
    auto       states      = PlyStateStack{};
    const auto initial_key = board.key();
    const int  depth =
        profile == Profile::Smoke ? perft_case.smoke_depth : perft_case.standard_depth;
    const NodeCount expected =
        profile == Profile::Smoke ? perft_case.smoke_nodes : perft_case.standard_nodes;
    NodeCount nodes = 0;

    const std::uint64_t total_ns =
        measure_ns([&] { nodes = perft_nodes(board, depth, states, 0, root); });

    if (nodes != expected) {
        std::ostringstream message;
        message << "perft mismatch for " << perft_case.id << " depth " << depth << ": got " << nodes
                << ", expected " << expected;
        throw std::runtime_error(message.str());
    }
    if (board.key() != initial_key || board.calculate_key() != initial_key)
        throw std::runtime_error("board restoration failed for " + std::string(perft_case.id));

    const double seconds = static_cast<double>(total_ns) / 1'000'000'000.0;
    return {
        .case_id        = std::string(perft_case.id),
        .profile        = to_string(profile),
        .depth          = depth,
        .nodes          = nodes,
        .expected_nodes = expected,
        .total_ns       = total_ns,
        .nodes_per_sec  = seconds > 0.0 ? static_cast<double>(nodes) / seconds : 0.0,
    };
}

void emit_tsv(const std::vector<PerftRow>& rows) {
    std::cout << "result_format\tcase\tprofile\tdepth\tnodes\texpected_nodes\ttotal_ns\t"
                 "nodes_per_sec\n";
    for (const auto& row : rows) {
        std::cout << "perft\t" << row.case_id << '\t' << row.profile << '\t' << row.depth << '\t'
                  << row.nodes << '\t' << row.expected_nodes << '\t' << row.total_ns << '\t'
                  << std::fixed << std::setprecision(3) << row.nodes_per_sec << '\n';
    }
}

void emit_text(const std::vector<PerftRow>& rows) {
    for (const auto& row : rows) {
        const double total_ms = static_cast<double>(row.total_ns) / 1'000'000.0;
        std::cout << row.case_id << " depth " << row.depth << ": " << row.nodes << " nodes in "
                  << std::fixed << std::setprecision(3) << total_ms << " ms ("
                  << std::setprecision(0) << row.nodes_per_sec << " nps)\n";
    }
}

Profile parse_profile(const std::string& value) {
    if (value == "smoke")
        return Profile::Smoke;
    if (value == "standard")
        return Profile::Standard;
    throw std::runtime_error("unknown profile: " + value);
}

OutputFormat parse_format(const std::string& value) {
    if (value == "text")
        return OutputFormat::Text;
    if (value == "tsv")
        return OutputFormat::Tsv;
    throw std::runtime_error("unknown format: " + value);
}

void print_usage(const char* argv0) {
    std::cerr << "Perft benchmark for recursive move generation validation.\n";
    std::cerr << "Usage: " << argv0 << " [--profile smoke|standard] [--format text|tsv]\n";
}

Options parse_args(int argc, char* argv[]) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }

        if (arg == "--profile") {
            if (++i >= argc)
                throw std::runtime_error("missing value for --profile");
            options.profile = parse_profile(argv[i]);
            continue;
        }

        if (arg == "--format") {
            if (++i >= argc)
                throw std::runtime_error("missing value for --format");
            options.format = parse_format(argv[i]);
            continue;
        }

        throw std::runtime_error("unknown argument: " + arg);
    }

    return options;
}

} // namespace

int main(int argc, char* argv[]) {
    attacks::init();

    try {
        const Options         options = parse_args(argc, argv);
        std::vector<PerftRow> rows;
        for (const auto& perft_case : perft_cases(options.profile))
            rows.push_back(run_perft_case(perft_case, options.profile));

        if (options.format == OutputFormat::Tsv)
            emit_tsv(rows);
        else
            emit_text(rows);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
