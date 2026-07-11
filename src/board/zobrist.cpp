#include "board/zobrist.hpp"

#include <random>

namespace zob {

PositionKey piece[N_COLORS][N_PIECETYPES][N_SQUARES];
PositionKey turn;
PositionKey ep[8];
PositionKey castle[2][N_COLORS];

void init() {
    std::mt19937_64 r;

    for (int i = 0; i < N_COLORS; i++)
        for (int j = 0; j < N_PIECETYPES; j++)
            for (int k = 0; k < N_SQUARES; k++)
                piece[i][j][k] = r();

    turn = r();

    for (int i = 0; i < 8; i++)
        ep[i] = r();

    for (int i = 0; i < N_COLORS; i++) {
        castle[CASTLE_KINGSIDE][i]  = r();
        castle[CASTLE_QUEENSIDE][i] = r();
    }
}

} // namespace zob
