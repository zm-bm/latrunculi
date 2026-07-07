#include "uci/uci_writer.hpp"

#include "board/board.hpp"
#include "board/position_state.hpp"
#include "core/defs.hpp"
#include "eval/evaluator.hpp"
#include "search/root_line.hpp"
#include "search/search_instrumentation.hpp"

#include <cstdlib>
#include <format>
#include <utility>

namespace uci {

namespace {

std::string format_score(int score) {
    if (std::abs(score) > MATE_BOUND) {
        int mate_distance = MATE_VALUE - std::abs(score);
        int mate_in_n     = (mate_distance + 1) / 2;
        return "mate " + std::to_string(mate_in_n * (score > 0 ? 1 : -1));
    }
    return "cp " + std::to_string(score);
}

std::string format_nps(uint64_t nodes, Milliseconds time) {
    auto count = time.count();
    auto nps   = count > 0 ? (nodes * 1000 / count) : 0;
    return std::to_string(nps);
}

std::string format_root_pv(const RootLine& line, const Board& root_board) {
    if (!line.usable_root_move() || line.pv.empty() || line.pv.front() != line.root_move)
        return "";

    PositionStateStack pv_states;
    Board              pv_board{pv_states.root()};
    pv_board.load_board(&root_board);

    std::string pv;
    for (int i = 0; i < line.pv.size(); ++i) {
        const Move move = line.pv.move_at(i);
        if (!pv_board.is_legal_move(move))
            return "";

        if (!pv.empty())
            pv += ' ';
        pv += move.str();

        if (i + 1 < line.pv.size())
            pv_board.make(move, pv_states.child(i));
    }

    return pv;
}

std::string format_search_info(const RootLine& line,
                               const Board&    root_board,
                               uint64_t        nodes,
                               Milliseconds    time) {
    std::string info = std::format("info depth {} score {} nodes {} time {} nps {}",
                                   line.depth,
                                   format_score(line.value),
                                   nodes,
                                   time.count(),
                                   format_nps(nodes, time));

    const std::string pv = format_root_pv(line, root_board);
    if (!pv.empty())
        info += " pv " + pv;

    return info;
}

} // namespace

std::string format_uci_move(Move move) {
    return move.is_null() ? "0000" : move.str();
}

std::string format_option(std::string_view name, const SpinOption& opt) {
    return std::format("option name {} type spin default {} min {} max {}",
                       name,
                       opt.default_value,
                       opt.min_value,
                       opt.max_value);
}

std::string format_option(std::string_view name, const CheckOption& opt) {
    return std::format(
        "option name {} type check default {}", name, opt.default_value ? "true" : "false");
}

std::string format_option(std::string_view name, const ButtonOption&) {
    return std::format("option name {} type button", name);
}

std::string format_identification(const uci::Options& options) {
    return std::format("id name Latrunculi {}\n"
                       "id author Eric VanderHelm\n"
                       "{}\n"
                       "{}\n"
                       "{}\n"
                       "{}\n"
                       "uciok",
                       LATRUNCULI_VERSION,
                       format_option("Hash", options.hash),
                       format_option("Clear Hash", options.clear_hash),
                       format_option("Threads", options.threads),
                       format_option("Debug", options.debug));
}

std::string format_ready() {
    return "readyok";
}

std::string format_bestmove(Move move) {
    return std::format("bestmove {}", format_uci_move(move));
}

std::string format_info_string(std::string_view str) {
    std::string sanitized{str};
    for (char& c : sanitized) {
        if (c == '\r' || c == '\n')
            c = ' ';
    }
    return std::format("info string {}", sanitized);
}

void Writer::help() const {
    constexpr auto format_str = R"(
Available commands:
  uci           - Show engine identity and supported options
  isready       - Check if the engine is ready
  setoption     - Set engine options
  ucinewgame    - Start a new game
  position      - Set up the board position
  go            - Start searching for the best move
  stop          - Stop the search
  ponderhit     - Handle ponder hit
  quit          - Exit the engine
  perft <depth> - Run perft for the given depth
  move <move>   - Make a move on the board
  moves         - Show all legal moves
  d / board     - Display the current board position
  eval          - Evaluate the current position
For UCI protocol details, see: reference/uci-protocol-specification.txt)";
    err << format_str << '\n';
    err.flush();
}

void Writer::identify(const uci::Options& options) const {
    out << format_identification(options) << '\n';
    out.flush();
}

void Writer::ready() const {
    out << format_ready() << '\n';
    out.flush();
}

void Writer::bestmove(Move move) const {
    out << format_bestmove(move) << '\n';
    out.flush();
}

void Writer::search_info(const RootLine& line,
                         const Board&    root_board,
                         uint64_t        nodes,
                         Milliseconds    time) const {
    out << format_search_info(line, root_board, nodes, time) << '\n';
    out.flush();
}

void Writer::info_string(std::string_view str) const {
    out << format_info_string(str) << '\n';
    out.flush();
}

void Writer::debug_text(std::string_view str) const {
    err << str;
    err.flush();
}

template <typename T>
void Writer::debug(T&& obj) const {
    err << std::format("{}", std::forward<T>(obj)) << '\n';
    err.flush();
}

template void Writer::debug(std::string& str) const;
template void Writer::debug(std::string&& str) const;
template void Writer::debug(Board& board) const;
template void Writer::debug(EvaluatorDebug& evaluator) const;
template void Writer::debug(SearchInstrumentation<>& evaluator) const;

} // namespace uci
