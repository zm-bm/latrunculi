#include "zobrist.hpp"

#include <random>

namespace Zobrist {

U64 psq[N_COLORS][N_PIECES][N_SQUARES];
U64 stm;
U64 ep[8];
U64 castle[N_COLORS][N_CASTLES];

void init() {
    std::mt19937_64 r;

    for (int i = 0; i < N_COLORS; i++)
        for (int j = 0; j < N_PIECES; j++)
            for (int k = 0; k < N_SQUARES; k++) psq[i][j][k] = r();

    stm = r();

    for (int i = 0; i < 8; i++) ep[i] = r();

    for (int i = 0; i < N_COLORS; i++) {
        castle[i][0] = r();
        castle[i][1] = r();
    }
}

}  // namespace Zobrist