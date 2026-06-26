#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <limits.h>
#include <unistd.h>
#endif

#include "board.hpp"
#include "engine.hpp"
#include "magic.hpp"
#include "movegen.hpp"
#include "zobrist.hpp"

namespace {

using BenchClock = std::chrono::steady_clock;

constexpr int HASH_MB = 16;

constexpr const char* STARTFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr const char* POS2 = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
constexpr const char* POS3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
constexpr const char* POS4W = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
constexpr const char* POS4B = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1";
constexpr const char* POS5  = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
constexpr const char* POS6 =
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";
constexpr const char* SPECIAL_PROMOTION         = "4k3/P6p/8/8/8/8/p6P/4K3 w - - 0 1";
constexpr const char* SPECIAL_CAPTURE_PROMOTION = "1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1";
constexpr const char* SPECIAL_EP                = "4k3/8/8/8/Pp6/8/8/4K3 b - a3 0 1";
constexpr const char* SPECIAL_CASTLE            = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
constexpr const char* SPECIAL_EVASION           = "k3r3/8/8/8/8/8/8/2B1K1N1 w - - 0 1";

volatile uint64_t bench_sink = 0;

enum class Suite { SearchSmoke, BoardCore, Perft };
enum class OutputFormat { Text, Tsv };
enum class Profile { Smoke, Standard };

struct Options {
    Suite        suite{Suite::SearchSmoke};
    OutputFormat format{OutputFormat::Text};
    Profile      profile{Profile::Smoke};
    std::string  testfile;
    int          searchtime_ms{1000};
    int          threads{1};
    int          limit{5};
};

struct FenCase {
    std::string_view id;
    std::string_view fen;
};

struct PerftCase {
    std::string_view id;
    std::string_view fen;
    int              smoke_depth;
    uint64_t         smoke_nodes;
    int              standard_depth;
    uint64_t         standard_nodes;
};

struct SpecialMoveCase {
    std::string_view id;
    std::string_view mode;
    std::string_view fen;
    Move             move;
};

struct BenchRow {
    std::string suite;
    std::string case_id;
    std::string mode;
    std::string profile;
    uint64_t    iterations{0};
    uint64_t    ops{0};
    uint64_t    total_ns{0};
    double      ns_per_op{0.0};
    double      nodes_per_sec{0.0};
    double      moves_per_op{0.0};
};

struct SearchSmokeCase {
    std::string fen;
    std::string source_text;

    explicit SearchSmokeCase(std::string line) {
        const auto metadata_pos = line.find(';');
        source_text = metadata_pos != std::string::npos ? line.substr(0, metadata_pos) : line;

        std::istringstream         input(source_text);
        std::array<std::string, 4> fen_fields;
        for (auto& field : fen_fields) {
            if (!(input >> field))
                throw std::runtime_error("invalid search-smoke EPD line: " + source_text);
        }

        fen = fen_fields[0] + " " + fen_fields[1] + " " + fen_fields[2] + " " + fen_fields[3];
    }
};

struct SearchSmokeInfo {
    int         depth = 0;
    int         time  = 0;
    uint64_t    nodes = 0;
    uint64_t    nps   = 0;
    std::string first_move;

    explicit SearchSmokeInfo(const std::string& line) {
        std::istringstream input(line);
        std::string        token;

        while (input >> token) {
            if (token == "depth") {
                if (input >> token)
                    depth = std::stoi(token);
            } else if (token == "time") {
                if (input >> token)
                    time = std::stoi(token);
            } else if (token == "nodes") {
                if (input >> token)
                    nodes = std::stoull(token);
            } else if (token == "nps") {
                if (input >> token)
                    nps = std::stoull(token);
            } else if (token == "pv") {
                if (input >> token)
                    first_move = token;
            }
        }
    }
};

struct SearchSmokeResult {
    bool            saw_observed_info      = false;
    bool            has_bestmove           = false;
    bool            has_plausible_bestmove = false;
    int             observed_max_depth     = 0;
    int             observed_max_time      = 0;
    uint64_t        observed_max_nodes     = 0;
    uint64_t        observed_nps           = 0;
    std::string     bestmove;
    SearchSmokeCase test_case;

    explicit SearchSmokeResult(SearchSmokeCase test_case_) : test_case(std::move(test_case_)) {}

    static bool is_plausible_move(const std::string& fen, const std::string& move) {
        if (move.empty() || move == "(none)")
            return false;

        PositionState position_state;
        Board         board(position_state, fen);
        MoveList      movelist = generate<ALL_MOVES>(board);

        auto move_matches = [&](Move candidate) { return candidate.str() == move; };
        return std::find_if(movelist.begin(), movelist.end(), move_matches) != movelist.end();
    }

    void observe(const SearchSmokeInfo& info) {
        saw_observed_info  = true;
        observed_max_depth = std::max(observed_max_depth, info.depth);
        observed_max_time  = std::max(observed_max_time, info.time);
        observed_max_nodes = std::max(observed_max_nodes, info.nodes);
        observed_nps       = info.nps;
    }
};

std::ostream& operator<<(std::ostream& os, const SearchSmokeResult& result) {
    os << "Observed: ";
    os << "bestmove ";
    if (result.has_bestmove) {
        os << result.bestmove;
        if (result.has_plausible_bestmove)
            os << " (plausible)";
    } else {
        os << "missing";
    }
    os << " | depth " << result.observed_max_depth;
    os << " | nodes " << result.observed_max_nodes;
    os << " | time " << result.observed_max_time << " ms";
    os << " | nps " << result.observed_nps;
    os << " | fen " << result.test_case.source_text;
    return os;
}

std::string to_string(Profile profile) {
    return profile == Profile::Smoke ? "smoke" : "standard";
}

std::string format_double(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << value;
    return out.str();
}

template <typename T>
double average_of(const std::vector<T>& values) {
    if (values.empty())
        return 0.0;

    const auto total = std::reduce(values.begin(), values.end(), static_cast<long double>(0));
    return static_cast<double>(total / values.size());
}

template <typename T>
double median_of(std::vector<T> values) {
    if (values.empty())
        return 0.0;

    std::sort(values.begin(), values.end());
    const size_t middle = values.size() / 2;
    if (values.size() % 2 == 1)
        return static_cast<double>(values[middle]);

    return static_cast<double>(values[middle - 1] + values[middle]) / 2.0;
}

template <typename T>
std::pair<T, T> minmax_of(const std::vector<T>& values) {
    if (values.empty())
        return {T{}, T{}};

    const auto [min_it, max_it] = std::minmax_element(values.begin(), values.end());
    return {*min_it, *max_it};
}

std::string format_metric(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(value >= 100.0 ? 0 : 1) << value;
    return out.str();
}

std::string get_test_file_path() {
    std::vector<std::filesystem::path> candidates;

#ifdef LATRUNCULI_SOURCE_DIR
    candidates.emplace_back(std::filesystem::path(LATRUNCULI_SOURCE_DIR) / "bench" /
                            "arasan20.epd");
#endif

#ifndef _WIN32
    char    buffer[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
    if (count != -1) {
        std::filesystem::path exe_path(std::string(buffer, count));
        std::filesystem::path exe_dir = exe_path.parent_path();
        candidates.emplace_back(exe_dir / "../../bench/arasan20.epd");
        candidates.emplace_back(exe_dir / "../bench/arasan20.epd");
    }
#endif

    candidates.emplace_back(std::filesystem::current_path() / "bench" / "arasan20.epd");

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate))
            return std::filesystem::canonical(candidate).string();
    }

    std::ostringstream message;
    message << "Test file not found. Checked:";
    for (const auto& candidate : candidates)
        message << "\n  - " << candidate.string();
    throw std::runtime_error(message.str());
}

std::vector<FenCase> board_cases(Profile profile) {
    std::vector<FenCase> cases = {
        {"startpos", STARTFEN},
        {"pos2", POS2},
        {"pos3", POS3},
        {"special", SPECIAL_CASTLE},
    };

    if (profile == Profile::Standard) {
        cases.push_back({"pos4w", POS4W});
        cases.push_back({"pos4b", POS4B});
        cases.push_back({"pos5", POS5});
        cases.push_back({"pos6", POS6});
        cases.push_back({"promotion", SPECIAL_PROMOTION});
        cases.push_back({"en-passant", SPECIAL_EP});
    }

    return cases;
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

uint64_t iterations_for(Profile profile) {
    return profile == Profile::Smoke ? 20000 : 200000;
}

uint64_t null_iterations_for(Profile profile) {
    return profile == Profile::Smoke ? 100000 : 1000000;
}

std::vector<Move> legal_moves(Board& board) {
    std::vector<Move> moves;
    MoveList          movelist = generate<ALL_MOVES>(board);
    for (const auto& move : movelist) {
        if (board.is_legal_pseudo_move(move))
            moves.push_back(move);
    }
    return moves;
}

std::vector<Move> pseudo_moves(Board& board) {
    std::vector<Move> moves;
    MoveList          movelist = generate<ALL_MOVES>(board);
    for (const auto& move : movelist)
        moves.push_back(move);
    return moves;
}

std::vector<Move> pseudo_quiet_moves(Board& board) {
    std::vector<Move> moves;
    for (const auto& move : pseudo_moves(board)) {
        if (move.type() != MOVE_PROM && !board.is_capture(move))
            moves.push_back(move);
    }
    return moves;
}

std::vector<Move> pseudo_capture_moves(Board& board) {
    std::vector<Move> moves;
    for (const auto& move : pseudo_moves(board)) {
        if (move.type() != MOVE_PROM && move.type() != MOVE_EP && board.is_capture(move))
            moves.push_back(move);
    }
    return moves;
}

std::vector<Move> pseudo_king_moves(Board& board) {
    std::vector<Move> moves;
    const Square      king = board.king_sq(board.side_to_move());
    for (const auto& move : pseudo_moves(board)) {
        if (move.from() == king)
            moves.push_back(move);
    }
    return moves;
}

std::vector<Move> pseudo_ep_moves(Board& board) {
    std::vector<Move> moves;
    for (const auto& move : pseudo_moves(board)) {
        if (move.type() == MOVE_EP)
            moves.push_back(move);
    }
    return moves;
}

std::vector<Move> pseudo_evasion_moves(Board& board) {
    std::vector<Move> moves;
    if (!board.is_check())
        return moves;

    MoveList movelist = generate<EVASIONS>(board);
    for (const auto& move : movelist)
        moves.push_back(move);
    return moves;
}

std::vector<Move> quiet_moves(Board& board) {
    std::vector<Move> moves;
    for (const auto& move : legal_moves(board)) {
        if (move.type() != MOVE_PROM && !board.is_capture(move))
            moves.push_back(move);
    }
    return moves;
}

std::vector<Move> capture_moves(Board& board) {
    std::vector<Move> moves;
    for (const auto& move : legal_moves(board)) {
        if (move.type() != MOVE_PROM && board.is_capture(move))
            moves.push_back(move);
    }
    return moves;
}

template <typename Fn>
uint64_t measure_ns(Fn&& fn) {
    const auto start = BenchClock::now();
    fn();
    const auto end = BenchClock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
}

BenchRow make_row(const std::string& suite,
                  const std::string& case_id,
                  const std::string& mode,
                  Profile            profile,
                  uint64_t           iterations,
                  uint64_t           ops,
                  uint64_t           total_ns,
                  double             nodes_per_sec,
                  double             moves_per_op) {
    const double ns_per_op =
        ops > 0 ? static_cast<double>(total_ns) / static_cast<double>(ops) : 0.0;
    return BenchRow{
        .suite         = suite,
        .case_id       = case_id,
        .mode          = mode,
        .profile       = to_string(profile),
        .iterations    = iterations,
        .ops           = ops,
        .total_ns      = total_ns,
        .ns_per_op     = ns_per_op,
        .nodes_per_sec = nodes_per_sec,
        .moves_per_op  = moves_per_op,
    };
}

uint64_t perft_nodes(Board&                                         board,
                     int                                            depth,
                     std::array<PositionState, MAX_SEARCH_PLY + 1>& states,
                     int                                            ply,
                     PositionState&                                 current_state) {
    if (depth == 0)
        return 1;

    uint64_t nodes    = 0;
    auto     movelist = generate<ALL_MOVES>(board);

    for (auto& move : movelist) {
        if (!board.is_legal_pseudo_move(move))
            continue;

        board.make(move, states[ply + 1]);
        nodes += perft_nodes(board, depth - 1, states, ply + 1, states[ply + 1]);
        board.unmake(current_state);
    }

    return nodes;
}

void assert_restored(const Board& board, uint64_t initial_key, const std::string& label) {
    if (board.key() != initial_key || board.calculate_key() != initial_key)
        throw std::runtime_error("board restoration failed for " + label);
}

std::vector<BenchRow>
run_generate_mode(const FenCase& fen_case, Profile profile, GeneratorMode mode) {
    PositionState root;
    Board         board(root, std::string(fen_case.fen));
    const auto    iterations  = iterations_for(profile);
    uint64_t      total_moves = 0;

    const uint64_t total_ns = measure_ns([&] {
        for (uint64_t i = 0; i < iterations; ++i) {
            MoveList movelist;
            if (mode == ALL_MOVES)
                movelist = generate<ALL_MOVES>(board);
            else
                movelist = generate<CAPTURES>(board);

            total_moves += movelist.size();
            bench_sink  += movelist.size();
        }
    });

    const std::string mode_name = mode == ALL_MOVES ? "generate_all" : "generate_captures";
    return {make_row("board-core",
                     std::string(fen_case.id),
                     mode_name,
                     profile,
                     iterations,
                     iterations,
                     total_ns,
                     0.0,
                     static_cast<double>(total_moves) / static_cast<double>(iterations))};
}

BenchRow run_legal_filter_case(const FenCase& fen_case, Profile profile) {
    PositionState root;
    Board         board(root, std::string(fen_case.fen));
    const auto    iterations  = iterations_for(profile);
    uint64_t      total_legal = 0;

    const uint64_t total_ns = measure_ns([&] {
        for (uint64_t i = 0; i < iterations; ++i) {
            MoveList movelist = generate<ALL_MOVES>(board);
            uint64_t legal    = 0;
            for (const auto& move : movelist)
                legal += board.is_legal_pseudo_move(move) ? 1 : 0;
            total_legal += legal;
            bench_sink  += legal;
        }
    });

    return make_row("board-core",
                    std::string(fen_case.id),
                    "legal_filter_all",
                    profile,
                    iterations,
                    iterations,
                    total_ns,
                    0.0,
                    static_cast<double>(total_legal) / static_cast<double>(iterations));
}

std::optional<BenchRow> run_prebuilt_legal_filter_case(const FenCase&     fen_case,
                                                       Profile            profile,
                                                       const std::string& mode,
                                                       std::vector<Move> (*move_selector)(Board&)) {
    PositionState root;
    Board         board(root, std::string(fen_case.fen));
    const auto    moves = move_selector(board);

    if (moves.empty())
        return std::nullopt;

    const auto iterations  = iterations_for(profile);
    const auto ops         = iterations * moves.size();
    uint64_t   total_legal = 0;

    const uint64_t total_ns = measure_ns([&] {
        for (uint64_t i = 0; i < iterations; ++i) {
            uint64_t legal = 0;
            for (Move move : moves)
                legal += board.is_legal_pseudo_move(move) ? 1 : 0;
            total_legal += legal;
            bench_sink  += legal;
        }
    });

    return make_row("board-core",
                    std::string(fen_case.id),
                    mode,
                    profile,
                    iterations,
                    ops,
                    total_ns,
                    0.0,
                    static_cast<double>(moves.size()));
}

BenchRow run_make_unmake_case(const FenCase&     fen_case,
                              Profile            profile,
                              const std::string& mode,
                              std::vector<Move> (*move_selector)(Board&)) {
    PositionState root;
    PositionState next;
    Board         board(root, std::string(fen_case.fen));
    const auto    initial_key = board.key();
    const auto    moves       = move_selector(board);

    if (moves.empty())
        return make_row("board-core", std::string(fen_case.id), mode, profile, 0, 0, 0, 0.0, 0.0);

    const auto iterations = iterations_for(profile);
    const auto ops        = iterations * moves.size();

    const uint64_t total_ns = measure_ns([&] {
        for (uint64_t i = 0; i < iterations; ++i) {
            for (Move move : moves) {
                board.make(move, next);
                bench_sink += board.key() & 1ULL;
                board.unmake(root);
            }
        }
    });

    assert_restored(board, initial_key, std::string(fen_case.id) + "/" + mode);
    return make_row("board-core",
                    std::string(fen_case.id),
                    mode,
                    profile,
                    iterations,
                    ops,
                    total_ns,
                    0.0,
                    static_cast<double>(moves.size()));
}

BenchRow run_special_move_case(const SpecialMoveCase& special_case, Profile profile) {
    PositionState root;
    PositionState next;
    Board         board(root, std::string(special_case.fen));
    const auto    initial_key = board.key();

    if (!board.is_pseudo_legal(special_case.move) || !board.is_legal_pseudo_move(special_case.move))
        throw std::runtime_error("special benchmark move is not legal: " +
                                 std::string(special_case.id));

    const auto iterations = iterations_for(profile);

    const uint64_t total_ns = measure_ns([&] {
        for (uint64_t i = 0; i < iterations; ++i) {
            board.make(special_case.move, next);
            bench_sink += board.key() & 1ULL;
            board.unmake(root);
        }
    });

    assert_restored(board, initial_key, std::string(special_case.id));
    return make_row("board-core",
                    std::string(special_case.id),
                    std::string(special_case.mode),
                    profile,
                    iterations,
                    iterations,
                    total_ns,
                    0.0,
                    1.0);
}

BenchRow run_null_move_case(const FenCase& fen_case, Profile profile) {
    PositionState root;
    PositionState next;
    Board         board(root, std::string(fen_case.fen));
    const auto    initial_key = board.key();
    const auto    iterations  = null_iterations_for(profile);

    const uint64_t total_ns = measure_ns([&] {
        for (uint64_t i = 0; i < iterations; ++i) {
            board.make_null(next);
            bench_sink += board.key() & 1ULL;
            board.unmake_null(root);
        }
    });

    assert_restored(board, initial_key, std::string(fen_case.id) + "/null_make_unmake");
    return make_row("board-core",
                    std::string(fen_case.id),
                    "null_make_unmake",
                    profile,
                    iterations,
                    iterations,
                    total_ns,
                    0.0,
                    1.0);
}

BenchRow run_perft_case(const PerftCase& perft_case, Profile profile) {
    PositionState root;
    Board         board(root, std::string(perft_case.fen));
    auto          states      = std::array<PositionState, MAX_SEARCH_PLY + 1>{};
    const auto    initial_key = board.key();
    const int     depth =
        profile == Profile::Smoke ? perft_case.smoke_depth : perft_case.standard_depth;
    const uint64_t expected =
        profile == Profile::Smoke ? perft_case.smoke_nodes : perft_case.standard_nodes;
    uint64_t nodes = 0;

    const uint64_t total_ns =
        measure_ns([&] { nodes = perft_nodes(board, depth, states, 0, root); });

    if (nodes != expected) {
        std::ostringstream message;
        message << "perft mismatch for " << perft_case.id << " depth " << depth << ": got " << nodes
                << ", expected " << expected;
        throw std::runtime_error(message.str());
    }
    assert_restored(board, initial_key, std::string(perft_case.id) + "/perft");

    const double seconds = static_cast<double>(total_ns) / 1'000'000'000.0;
    return make_row("perft",
                    std::string(perft_case.id),
                    "depth_" + std::to_string(depth),
                    profile,
                    1,
                    nodes,
                    total_ns,
                    seconds > 0.0 ? static_cast<double>(nodes) / seconds : 0.0,
                    0.0);
}

void print_tsv(const std::vector<BenchRow>& rows) {
    std::cout << "suite\tcase\tmode\tprofile\titerations\tops\ttotal_ns\tns_per_op\tnodes_per_"
                 "sec\tmoves_per_op\n";
    for (const auto& row : rows) {
        std::cout << row.suite << '\t' << row.case_id << '\t' << row.mode << '\t' << row.profile
                  << '\t' << row.iterations << '\t' << row.ops << '\t' << row.total_ns << '\t'
                  << format_double(row.ns_per_op) << '\t' << format_double(row.nodes_per_sec)
                  << '\t' << format_double(row.moves_per_op) << '\n';
    }
}

void print_text(const std::vector<BenchRow>& rows) {
    for (const auto& row : rows) {
        std::cout << row.suite << " " << row.case_id << " " << row.mode << ": "
                  << format_double(row.ns_per_op) << " ns/op";
        if (row.nodes_per_sec > 0.0)
            std::cout << ", " << format_double(row.nodes_per_sec) << " nodes/sec";
        if (row.moves_per_op > 0.0)
            std::cout << ", " << format_double(row.moves_per_op) << " moves/op";
        std::cout << '\n';
    }
}

void emit_rows(const std::vector<BenchRow>& rows, OutputFormat format) {
    if (format == OutputFormat::Tsv)
        print_tsv(rows);
    else
        print_text(rows);
}

} // namespace

class Benchmark {
private:
    std::vector<SearchSmokeCase> test_cases;

    std::ostringstream oss;
    std::stringstream  iss;
    Engine             engine{oss, oss, iss};
    Options            options;

public:
    explicit Benchmark(Options opts) : options(std::move(opts)) {
        engine.execute("setoption name Threads value " + std::to_string(options.threads));
        engine.execute("setoption name Hash value " + std::to_string(HASH_MB));

        std::ifstream file(options.testfile);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line))
                test_cases.push_back(SearchSmokeCase(line));
        } else {
            throw std::runtime_error("could not open search-smoke test file: " + options.testfile);
        }
    }

    SearchSmokeResult test_search(SearchSmokeCase& test_case) {
        SearchSmokeResult result{test_case};
        oss.str("");
        oss.clear();

        engine.execute("position fen " + test_case.fen);
        engine.execute("go movetime " + std::to_string(options.searchtime_ms));
        while (true) {
            if (oss.str().find("bestmove") != std::string::npos)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::istringstream lines{oss.str()};
        std::string        line;
        while (std::getline(lines, line)) {
            if (line.rfind("bestmove ", 0) == 0) {
                std::istringstream bestmove_line{line};
                std::string        token;
                bestmove_line >> token >> result.bestmove;
                result.has_bestmove = !result.bestmove.empty();
                result.has_plausible_bestmove =
                    SearchSmokeResult::is_plausible_move(test_case.fen, result.bestmove);
                continue;
            }

            if (line.find("info") == std::string::npos)
                continue;

            SearchSmokeInfo info{line};
            if (!info.first_move.empty())
                result.observe(info);
        }

        return result;
    }

    void run_all_tests() {
        std::vector<int>      observed_depths;
        std::vector<int>      observed_times;
        std::vector<uint64_t> observed_nodes;
        std::vector<uint64_t> observed_nps;
        int                   completed_cases = 0;
        int                   bestmove_cases  = 0;
        int                   plausible_cases = 0;

        for (auto& test_case : test_cases) {
            if (observed_depths.size() >= static_cast<size_t>(options.limit))
                break;

            SearchSmokeResult result = test_search(test_case);
            std::cout << result << std::endl;

            observed_depths.push_back(result.observed_max_depth);
            observed_times.push_back(result.observed_max_time);
            observed_nodes.push_back(result.observed_max_nodes);
            observed_nps.push_back(result.observed_nps);
            completed_cases += result.saw_observed_info ? 1 : 0;
            bestmove_cases  += result.has_bestmove ? 1 : 0;
            plausible_cases += result.has_plausible_bestmove ? 1 : 0;
        }

        const auto [min_depth, max_depth] = minmax_of(observed_depths);
        const auto [min_time, max_time]   = minmax_of(observed_times);
        const auto [min_nodes, max_nodes] = minmax_of(observed_nodes);
        const auto [min_nps, max_nps]     = minmax_of(observed_nps);

        std::cout << "\nSmoke Benchmark Summary\n";
        std::cout << "Threads = " << options.threads << ", ";
        std::cout << "movetime = " << options.searchtime_ms << " ms, ";
        std::cout << "cases = " << observed_depths.size() << std::endl;
        std::cout << "-------------------" << std::endl;
        std::cout << "Completed with observed info: " << completed_cases << " / "
                  << observed_depths.size() << ", bestmove seen: " << bestmove_cases << " / "
                  << observed_depths.size() << ", plausible bestmove: " << plausible_cases << " / "
                  << observed_depths.size() << std::endl;
        std::cout << "Observed averages: depth " << format_metric(average_of(observed_depths))
                  << " ply, nodes " << format_metric(average_of(observed_nodes)) << ", time "
                  << format_metric(average_of(observed_times)) << " ms, nps "
                  << format_metric(average_of(observed_nps)) << std::endl;
        std::cout << "Observed medians: nps " << format_metric(median_of(observed_nps))
                  << ", depth " << format_metric(median_of(observed_depths)) << " ply" << std::endl;
        std::cout << "Observed spread: depth " << min_depth << "-" << max_depth << " ply, nodes "
                  << min_nodes << "-" << max_nodes << ", time " << min_time << "-" << max_time
                  << " ms, nps " << min_nps << "-" << max_nps << std::endl;
        std::cout << "Use this as a quick stability/throughput smoke benchmark, not as a tactical "
                     "or comparative suite."
                  << std::endl;
    }
};

namespace {

void run_search_smoke(Options& options) {
    if (!options.testfile.empty()) {
        if (!std::filesystem::exists(options.testfile)) {
            std::cerr << "Warning: Provided test file does not exist: " << options.testfile
                      << std::endl;
            std::cerr << "Falling back to automatic path resolution." << std::endl;
            options.testfile = get_test_file_path();
        }
    } else {
        options.testfile = get_test_file_path();
    }

    std::cout << "Using test file: " << options.testfile << std::endl;
    std::cout << "Threads: " << options.threads << ", movetime: " << options.searchtime_ms
              << " ms, move limit: " << options.limit << std::endl;

    Benchmark benchmark{options};
    benchmark.run_all_tests();
}

void run_board_core(const Options& options) {
    std::vector<BenchRow> rows;
    const auto            cases                = board_cases(options.profile);
    const auto            push_attribution_row = [&](const FenCase&     fen_case,
                                          const std::string& mode,
                                          std::vector<Move> (*move_selector)(Board&)) {
        if (auto row =
                run_prebuilt_legal_filter_case(fen_case, options.profile, mode, move_selector))
            rows.push_back(*row);
    };

    for (const auto& fen_case : cases) {
        auto all_rows = run_generate_mode(fen_case, options.profile, ALL_MOVES);
        rows.insert(rows.end(), all_rows.begin(), all_rows.end());

        auto capture_rows = run_generate_mode(fen_case, options.profile, CAPTURES);
        rows.insert(rows.end(), capture_rows.begin(), capture_rows.end());

        rows.push_back(run_legal_filter_case(fen_case, options.profile));
        push_attribution_row(fen_case, "legal_filter_prebuilt_all", pseudo_moves);
        push_attribution_row(fen_case, "legal_filter_prebuilt_quiet", pseudo_quiet_moves);
        push_attribution_row(fen_case, "legal_filter_prebuilt_capture", pseudo_capture_moves);
        push_attribution_row(fen_case, "legal_filter_prebuilt_king", pseudo_king_moves);
        rows.push_back(
            run_make_unmake_case(fen_case, options.profile, "make_unmake_legal", legal_moves));
        rows.push_back(
            run_make_unmake_case(fen_case, options.profile, "make_unmake_quiet", quiet_moves));
        rows.push_back(
            run_make_unmake_case(fen_case, options.profile, "make_unmake_capture", capture_moves));
        rows.push_back(run_null_move_case(fen_case, options.profile));
    }

    push_attribution_row({"en-passant", SPECIAL_EP}, "legal_filter_prebuilt_ep", pseudo_ep_moves);
    push_attribution_row(
        {"evasion", SPECIAL_EVASION}, "legal_filter_prebuilt_evasion", pseudo_evasion_moves);

    for (const auto& special_case :
         {SpecialMoveCase{"promotion",
                          "make_unmake_promotion",
                          SPECIAL_PROMOTION,
                          Move(A7, A8, MOVE_PROM, QUEEN)},
          SpecialMoveCase{"capture-promotion",
                          "make_unmake_capture_promotion",
                          SPECIAL_CAPTURE_PROMOTION,
                          Move(A7, B8, MOVE_PROM, QUEEN)},
          SpecialMoveCase{
              "en-passant", "make_unmake_en_passant", SPECIAL_EP, Move(B4, A3, MOVE_EP)},
          SpecialMoveCase{
              "castle", "make_unmake_castle", SPECIAL_CASTLE, Move(E1, G1, MOVE_CASTLE)}})
        rows.push_back(run_special_move_case(special_case, options.profile));

    emit_rows(rows, options.format);
}

void run_perft(const Options& options) {
    std::vector<BenchRow> rows;
    for (const auto& perft_case : perft_cases(options.profile))
        rows.push_back(run_perft_case(perft_case, options.profile));
    emit_rows(rows, options.format);
}

Suite parse_suite(const std::string& value) {
    if (value == "search-smoke")
        return Suite::SearchSmoke;
    if (value == "board-core")
        return Suite::BoardCore;
    if (value == "perft")
        return Suite::Perft;
    throw std::runtime_error("unknown suite: " + value);
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
    std::cerr << "Benchmark suites for development sanity checks and board-core profiling.\n";
    std::cerr << "Usage: " << argv0
              << " [--suite search-smoke|board-core|perft] [--profile smoke|standard]\n"
              << "       [--format text|tsv] [--threads N] [--movetime MS] [--limit N]\n"
              << "       [path_to_search_smoke_test_file]\n";
}

Options parse_args(int argc, char* argv[]) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }

        if (arg == "--suite") {
            if (++i >= argc)
                throw std::runtime_error("missing value for --suite");
            options.suite = parse_suite(argv[i]);
            continue;
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

        if (arg == "--threads") {
            if (++i >= argc)
                throw std::runtime_error("missing value for --threads");
            options.threads = std::stoi(argv[i]);
            continue;
        }

        if (arg == "--movetime") {
            if (++i >= argc)
                throw std::runtime_error("missing value for --movetime");
            options.searchtime_ms = std::stoi(argv[i]);
            continue;
        }

        if (arg == "--limit") {
            if (++i >= argc)
                throw std::runtime_error("missing value for --limit");
            options.limit = std::stoi(argv[i]);
            continue;
        }

        options.testfile = arg;
    }

    if (options.threads < 1)
        throw std::runtime_error("--threads must be >= 1");
    if (options.searchtime_ms < 1)
        throw std::runtime_error("--movetime must be >= 1");
    if (options.limit < 1)
        throw std::runtime_error("--limit must be >= 1");

    return options;
}

} // namespace

int main(int argc, char* argv[]) {
    magic::init();
    zob::init();

    try {
        Options options = parse_args(argc, argv);

        switch (options.suite) {
        case Suite::SearchSmoke: run_search_smoke(options); break;
        case Suite::BoardCore:   run_board_core(options); break;
        case Suite::Perft:       run_perft(options); break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    bench_sink += 1;
    return 0;
}
