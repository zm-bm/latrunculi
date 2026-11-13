// Explicit offline neural verification, separate from UCI and HCE feature export.
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "board/board.hpp"
#include "core/notation.hpp"
#include "core/attacks.hpp"
#include "eval/nnue.hpp"
#include "movegen/moves.hpp"

int main() {
    try {
        attacks::init();
        std::string line;
        while (std::getline(std::cin, line)) {
            const auto separator = line.find('\t');
            Board board(line.substr(0, separator));
            auto check = [&] {
                if (board.neural_state() != nnue::refresh(board))
                    throw std::runtime_error("incremental/full-refresh mismatch");
            };
            check();
            if (separator != std::string::npos) {
                std::istringstream moves(line.substr(separator + 1));
                std::string move;
                while (moves >> move) {
                    if (move == "0000") {
                        if (board.is_check())
                            throw std::invalid_argument("null move in check");
                        board.make_null();
                    } else {
                        movegen::UnscoredMoves legal(board);
                        const Move parsed = legal.next_matching([&](Move m) { return m.str() == move; });
                        if (parsed.is_null())
                            throw std::invalid_argument("illegal history move");
                        board.make(parsed);
                    }
                    check();
                }
            }
            const auto score = nnue::evaluate(board.neural_state(), board.side_to_move());
            Board copy(board);
            check();
            if (copy.neural_state() != board.neural_state())
                throw std::runtime_error("Board copy mismatch");
            while (board.can_unmake()) {
                if (board.previous_move().is_null())
                    board.unmake_null();
                else
                    board.unmake();
                check();
            }
            std::cout << score << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
