#include "uci.hpp"

#include "board.hpp"
#include "evaluator.hpp"
#include "search_stats.hpp"

namespace uci {

void SpinOption::set(std::string value_str) {
    int val = std::stoi(value_str);
    if (val < min_value || val > max_value)
        throw std::out_of_range("Value out of range");
    value = val;
};

void CheckOption::set(std::string val_str) {
    value = (val_str == "true" || val_str == "on");
}

void Config::set_option(const std::string& name, const std::string& value) {
    if (name == "Hash") {
        hash.set(value);
        hash_callback(hash.value);
    } else if (name == "Threads") {
        threads.set(value);
        thread_callback(threads.value);
    } else if (name == "Debug") {
        debug.set(value);
    } else {
        throw std::invalid_argument("Unknown UCI option: " + name);
    }
}

std::string PV::get_score() const {
    if (std::abs(score) > MATE_BOUND) {
        int mate_distance = MATE_VALUE - std::abs(score);
        int mate_in_n     = (mate_distance + 1) / 2;
        return "mate " + std::to_string(mate_in_n * (score > 0 ? 1 : -1));
    }
    return "cp " + std::to_string(score);
}

std::string PV::get_nps() const {
    auto count = time.count();
    auto nps   = count > 0 ? (nodes * 1000 / count) : 0;
    return std::to_string(nps);
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
    constexpr auto format_str = R"(
id name Latrunculi {}
id author Eric VanderHelm
option name Hash {}
option name Threads {}
uciok)";
    out << std::format(format_str, LATRUNCULI_VERSION, config.hash, config.threads) << '\n';
}

void Protocol::ready() const {
    out << "readyok\n";
}

void Protocol::bestmove(std::string move) const {
    out << std::format("bestmove {}", move) << '\n';
}

void Protocol::info(const std::string& str) const {
    out << std::format("info string {}", str) << '\n';
}

void Protocol::info(const uci::PV& pv) const {
    out << std::format("info {}", pv) << '\n';
}

template <typename T>
void Protocol::diagnostic_output(T&& obj) const {
    err << std::format("{}", std::forward<T>(obj)) << '\n';
}

template void Protocol::diagnostic_output(std::string& str) const;
template void Protocol::diagnostic_output(std::string&& str) const;
template void Protocol::diagnostic_output(Board& board) const;
template void Protocol::diagnostic_output(EvaluatorDebug& evaluator) const;
template void Protocol::diagnostic_output(SearchStats<>& evaluator) const;

}; // namespace uci
