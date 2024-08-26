#include "uci.hpp"

#include <sstream>

#include "move.hpp"
#include "movegen.hpp"
#include "tt.hpp"

namespace UCI {

Controller::Controller(std::istream& is, std::ostream& os)
    : board(G::STARTFEN),
      search(&board),
      _debug(false),
      istream(is),
      ostream(os) {}

void Controller::loop() {
  std::string line;

  uci();

  while (std::getline(istream, line)) {
    // Break out of the game loop if execution fails
    if (!execute(line)) break;
  }
}

bool Controller::execute(const std::string& input) {
  // Split input string to get the UCI command
  auto tokens = G::split(input, ' ');
  auto cmd = tokens.at(0);
  tokens.erase(tokens.begin());

  if (cmd == "uci")
    uci();

  else if (cmd == "debug")
    setdebug(tokens);

  else if (cmd == "isready")
    ostream << "readyok" << std::endl;

  else if (cmd == "setoption")
    ostream << "TODO: implement uci options" << std::endl;

  else if (cmd == "ucinewgame")
    ostream << "TODO: clear transposition table" << std::endl;

  else if (cmd == "position")
    position(tokens);

  else if (cmd == "go")
    go(tokens);

  else if (cmd == "move")
    move(tokens);

  else if (cmd == "moves")
    moves();

  else if (cmd == "d")
    ostream << board << std::endl;

  else if (cmd == "eval")
    board.eval<true>();

  else if (cmd == "tt") {
    TT::Entry* ttEntry = TT::table.probe(board.getKey());
    if (ttEntry) ostream << *ttEntry;
  }

  else if (cmd == "quit" || cmd == "exit")
    return false;

  return true;
}

void Controller::uci() {
  // Identify the engine
  ostream << "id name Latrunculi 0.1.0" << std::endl;
  ostream << "id author Eric VanderHelm" << std::endl;
  ostream << "uciok" << std::endl;
}

void Controller::setdebug(std::vector<std::string>& tokens) {
  if (tokens.at(0) == "on")
    _debug = true;
  else if (tokens.at(0) == "off")
    _debug = false;
}

void Controller::position(std::vector<std::string>& tokens) {
  std::string pos = tokens.at(0);
  tokens.erase(tokens.begin());

  if (pos == "startpos") {
    board = Board(G::STARTFEN);
    search = Search(&board);

    if (_debug) ostream << board;
  } else if (pos == "fen") {
    std::string fen = "";

    for (auto& token : tokens) {
      if (token != "moves")
        fen += token + " ";
      else
        break;
    }

    board = Board(fen);
    search = Search(&board);

    if (_debug) ostream << board;
  } else
    return;

  ostream << "TODO: moves" << std::endl;
}

void Controller::go(std::vector<std::string>& tokens) {
  std::string mode = tokens.at(0);
  tokens.erase(tokens.begin());

  if (mode == "perft") {
    auto depth = std::stoi(tokens.at(0));
    search.perft<true>(depth);
  }

  else if (mode == "depth") {
    auto depth = std::stoi(tokens.at(0));
    search.think(depth);
  }
}

void Controller::move(std::vector<std::string>& tokens) {
  if (tokens.at(0) == "undo") {
    board.unmake();

    if (_debug) ostream << board;
  } else {
    auto gen = MoveGen::Generator(&board);
    gen.run();

    for (auto& move : gen.moves) {
      std::ostringstream oss;
      oss << move;

      if (oss.str() == tokens.at(0)) {
        board.make(move);

        if (_debug) ostream << board;
      }
    }
  }
}

void Controller::moves() {
  auto gen = MoveGen::Generator(&board);
  gen.run();

  TT::Entry* entry = TT::table.probe(board.getKey());
  if (entry)
    search.sortMoves(gen.moves, entry->best);
  else
    search.sortMoves(gen.moves);

  for (auto& move : gen.moves)
    ostream << move << ": " << move.score << std::endl;
}

}  // namespace UCI