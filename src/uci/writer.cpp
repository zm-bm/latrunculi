#include "uci/writer.hpp"

#include <cstdlib>
#include <format>

#include "board/board.hpp"
#include "board/notation.hpp"
#include "core/constants.hpp"
#include "search/root_line.hpp"
#include "uci/options.hpp"

namespace uci {

namespace {

std::string format_score(EvalValue score) {
    if (std::abs(score) > eval_value::mate_bound) {
        int mate_distance = eval_value::mate - std::abs(score);
        int mate_in_n     = (mate_distance + 1) / 2;
        return "mate " + std::to_string(mate_in_n * (score > 0 ? 1 : -1));
    }
    return "cp " + std::to_string(score);
}

std::string format_nps(NodeCount nodes, Milliseconds time) {
    auto count = time.count();
    auto nps   = count > 0 ? (nodes * 1000 / count) : 0;
    return std::to_string(nps);
}

std::string format_root_pv(const search::RootLine& line, const Board& root_board) {
    if (!line.usable_root_move() || line.pv.empty() || line.pv.front() != line.root_move)
        return "";

    Board pv_board(root_board);

    std::string pv;
    for (int i = 0; i < line.pv.size(); ++i) {
        const Move move = line.pv.move_at(i);
        if (!pv_board.is_legal_move(move))
            return "";

        if (!pv.empty())
            pv += ' ';
        pv += move.str();

        if (i + 1 < line.pv.size())
            pv_board.make(move);
    }

    return pv;
}

std::string format_search_info(const search::RootLine& line,
                               const Board&            root_board,
                               NodeCount               nodes,
                               Milliseconds            time) {
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

std::string format_identification(const Options& options) {
    return std::format("id name Latrunculi {}\n"
                       "id author Eric VanderHelm\n"
                       "{}\n"
                       "{}\n"
                       "{}\n"
                       "{}\n"
                       "uciok",
                       engine::version,
                       format_option("Hash", options.hash),
                       format_option("Clear Hash", options.clear_hash),
                       format_option("Threads", options.threads),
                       format_option("Ponder", options.ponder));
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

} // namespace

void Writer::write_text(std::ostream& stream, std::string_view text) const {
    std::lock_guard<std::mutex> lock(output_mutex);
    stream << text;
    stream.flush();
}

void Writer::write_line(std::ostream& stream, std::string_view text) const {
    std::lock_guard<std::mutex> lock(output_mutex);
    stream << text << '\n';
    stream.flush();
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
  eval          - Evaluate the current position)";
    write_line(diagnostics, format_str);
}

void Writer::identify(const Options& options) const {
    const std::string text = format_identification(options);
    write_line(output, text);
}

void Writer::ready() const {
    write_line(output, "readyok");
}

void Writer::report_progress(const search::RootLine& line,
                             const Board&            root_board,
                             NodeCount               nodes,
                             Milliseconds            time) {
    const std::string text = format_search_info(line, root_board, nodes, time);
    write_line(output, text);
}

void Writer::report_best_move(Move move) {
    const std::string text = format_bestmove(move);
    write_line(output, text);
}

void Writer::report_diagnostic(std::string_view text) {
    diagnostic_line(text);
}

void Writer::info_string(std::string_view str) const {
    const std::string text = format_info_string(str);
    write_line(output, text);
}

void Writer::diagnostic_line(std::string_view text) const {
    write_line(diagnostics, text);
}

void Writer::diagnostic_text(std::string_view text) const {
    write_text(diagnostics, text);
}

} // namespace uci
