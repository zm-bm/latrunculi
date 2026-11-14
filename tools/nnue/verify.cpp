// Offline verification protocol: FEN<TAB>optional UCI history, one JSON row out.
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "board/board.hpp"
#include "core/attacks.hpp"
#include "eval/nnue.hpp"
#include "movegen/moves.hpp"

int main(int argc, char** argv) {
    try {
        const bool all_backends = argc == 2 && std::string(argv[1]) == "--all-backends";
        if (argc > 1 && !all_backends)
            throw std::invalid_argument("expected --all-backends");
        attacks::init();
        (void)nnue::backend();
        std::string line;
        while (std::getline(std::cin, line)) {
            const auto separator = line.find('\t');
            Board board(line.substr(0, separator));
            const auto initial = board.neural_state();
            auto check = [&](bool layers = false) {
                const auto scalar = nnue::refresh(board, nnue::Backend::Scalar);
                if (board.neural_state() != scalar)
                    throw std::runtime_error("incremental/scalar-refresh accumulator mismatch");
                if (all_backends && nnue::avx2_available()) {
                    if (nnue::refresh(board, nnue::Backend::Avx2) != scalar)
                        throw std::runtime_error("AVX2/scalar accumulator mismatch");
                    if (layers)
                        for (Color turn : {BLACK, WHITE})
                            if (nnue::trace(nnue::network(), scalar, turn, nnue::Backend::Scalar)
                                != nnue::trace(nnue::network(), scalar, turn, nnue::Backend::Avx2))
                                throw std::runtime_error(
                                    "AVX2/scalar activation or output mismatch");
                }
            };
            check();
            if (separator != std::string::npos) {
                std::istringstream moves(line.substr(separator + 1));
                std::string token;
                while (moves >> token) {
                    if (token == "0000") {
                        if (board.is_check())
                            throw std::invalid_argument("null move in check");
                        board.make_null();
                    } else {
                        movegen::UnscoredMoves legal(board);
                        const Move move =
                            legal.next_matching([&](Move m) { return m.str() == token; });
                        if (move.is_null())
                            throw std::invalid_argument("illegal history move");
                        board.make(move);
                    }
                    check();
                }
            }
            check(true);
            const int raw = nnue::raw(board.neural_state(), board.side_to_move());
            Board fresh(board.to_fen());
            Board copy(board);
            if (fresh.neural_state() != board.neural_state()
                || copy.neural_state() != board.neural_state())
                throw std::runtime_error("fresh FEN or copy mismatch");
            if (nnue::raw(fresh.neural_state(), fresh.side_to_move()) != raw)
                throw std::runtime_error("fresh FEN score mismatch");
            while (board.can_unmake()) {
                if (board.previous_move().is_null())
                    board.unmake_null();
                else
                    board.unmake();
                check();
            }
            if (board.neural_state() != initial)
                throw std::runtime_error("unmake restoration mismatch");
            std::cout
                << "{\"raw\":"
                << raw
                << ",\"score\":"
                << nnue::adapt(raw)
                << ",\"network_sha256\":\""
                << nnue::network_sha256
                << "\",\"backend\":\""
                << nnue::backend_name()
                << "\"}\n";
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
