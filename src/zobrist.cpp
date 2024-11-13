#include "zobrist.hpp"
#include <random>

namespace Zobrist
{

    U64 psq[NCOLORS][N_PIECE_TYPES][NSQUARES];
    U64 stm;
    U64 ep[8];
    U64 castle[NCOLORS][2];

    void init()
    {
        std::mt19937_64 r;

        for (int i = 0; i < NCOLORS; i++)
            for (int j = 0; j < N_PIECE_TYPES; j++)
                for (int k = 0; k < NSQUARES; k++)
                    psq[i][j][k] = r();

        stm = r();

        for (int i = 0; i < 8; i++)
            ep[i] = r();

        for (int i = 0; i < NCOLORS; i++)
        {
            castle[i][0] = r();
            castle[i][1] = r();
        }
    }

}