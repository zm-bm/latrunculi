#include "uci.hpp"

#include "board.hpp"
#include "evaluator.hpp"
#include "position_state.hpp"
#include "root_line.hpp"
#include "search_instrumentation.hpp"

#include <cctype>
#include <cstdlib>
#include <format>
#include <utility>

namespace uci {

namespace {

std::string format_search_info_fields(const SearchInfo& info) {
    return std::format("depth {} score {} nodes {} time {} nps {} pv {}",
                       info.depth,
                       info.score_text(),
                       info.nodes,
                       info.time.count(),
                       info.nps_text(),
                       info.pv);
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

        pv += move.str() + " ";

        if (i + 1 < line.pv.size())
            pv_board.make(move, pv_states.child(i));
    }

    return pv;
}

} // namespace

std::string format_uci_move(Move move) {
    return move.is_null() ? "0000" : move.str();
}

std::string format_option(std::string_view name, const SpinOption& opt) {
    return std::format("option name {} type spin default {} min {} max {}",
                       name,
                       opt.def_value,
                       opt.min_value,
                       opt.max_value);
}

std::string format_option(std::string_view name, const CheckOption& opt) {
    return std::format(
        "option name {} type check default {}", name, opt.def_value ? "true" : "false");
}

std::string format_identification(const uci::Config& config) {
    return std::format("id name Latrunculi {}\n"
                       "id author Eric VanderHelm\n"
                       "{}\n"
                       "{}\n"
                       "{}\n"
                       "uciok",
                       LATRUNCULI_VERSION,
                       format_option("Hash", config.hash),
                       format_option("Threads", config.threads),
                       format_option("Debug", config.debug));
}

std::string format_ready() {
    return "readyok";
}

std::string format_bestmove(Move move) {
    return std::format("bestmove {}", format_uci_move(move));
}

std::string format_info(const uci::SearchInfo& info) {
    return std::format("info {}", format_search_info_fields(info));
}

std::string format_info_string(std::string_view str) {
    return std::format("info string {}", str);
}

void SpinOption::set(std::string value_str) {
    size_t pos = 0;
    int    val = std::stoi(value_str, &pos);
    while (pos < value_str.size() && std::isspace(static_cast<unsigned char>(value_str[pos])))
        ++pos;
    if (pos != value_str.size())
        throw std::invalid_argument("Invalid integer value");
    if (val < min_value || val > max_value)
        throw std::out_of_range("Value out of range");
    value = val;
}

void CheckOption::set(std::string val_str) {
    if (val_str == "true" || val_str == "on") {
        value = true;
    } else if (val_str == "false" || val_str == "off") {
        value = false;
    } else {
        throw std::invalid_argument("Invalid boolean value");
    }
}

void Config::set_option(const std::string& name, const std::string& value) {
    if (name == "Hash") {
        hash.set(value);
        if (hash_callback)
            hash_callback(hash.value);
    } else if (name == "Threads") {
        threads.set(value);
        if (thread_callback)
            thread_callback(threads.value);
    } else if (name == "Debug") {
        debug.set(value);
    } else {
        throw std::invalid_argument("Unknown UCI option: " + name);
    }
}

std::string SearchInfo::score_text() const {
    if (std::abs(score) > MATE_BOUND) {
        int mate_distance = MATE_VALUE - std::abs(score);
        int mate_in_n     = (mate_distance + 1) / 2;
        return "mate " + std::to_string(mate_in_n * (score > 0 ? 1 : -1));
    }
    return "cp " + std::to_string(score);
}

std::string SearchInfo::nps_text() const {
    auto count = time.count();
    auto nps   = count > 0 ? (nodes * 1000 / count) : 0;
    return std::to_string(nps);
}

SearchInfo
make_search_info(const RootLine& line, const Board& root_board, uint64_t nodes, Milliseconds time) {
    return SearchInfo{line.value, line.depth, nodes, time, format_root_pv(line, root_board)};
}

void Protocol::help() const {
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
  d             - Display the current board position
  eval          - Evaluate the current position
For UCI protocol details, see: https://www.wbec-ridderkerk.nl/html/UCIProtocol.html)";
    err << format_str << '\n';
}

void Protocol::identify(const uci::Config& config) const {
    out << format_identification(config) << '\n';
    out.flush();
}

void Protocol::ready() const {
    out << format_ready() << '\n';
    out.flush();
}

void Protocol::bestmove(Move move) const {
    out << format_bestmove(move) << '\n';
    out.flush();
}

void Protocol::info(const std::string& str) const {
    out << format_info_string(str) << '\n';
    out.flush();
}

void Protocol::info(const uci::SearchInfo& info) const {
    out << format_info(info) << '\n';
    out.flush();
}

template <typename T>
void Protocol::debug(T&& obj) const {
    err << std::format("{}", std::forward<T>(obj)) << '\n';
}

template void Protocol::debug(std::string& str) const;
template void Protocol::debug(std::string&& str) const;
template void Protocol::debug(Board& board) const;
template void Protocol::debug(EvaluatorDebug& evaluator) const;
template void Protocol::debug(SearchInstrumentation<>& evaluator) const;

} // namespace uci
