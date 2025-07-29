#include "zobrist.hpp"

#include <random>

namespace zob {

uint64_t piece[N_COLORS][N_PIECES][N_SQUARES];
uint64_t turn;
uint64_t ep[8];
uint64_t castle[2][N_COLORS];

void init() {
    std::mt19937_64 r;

    for (int i = 0; i < N_COLORS; i++)
        for (int j = 0; j < N_PIECES; j++)
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