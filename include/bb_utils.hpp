#pragma once

#include "types.hpp"

namespace BBUtils {

static constexpr BBTable createBBTable(auto func) {
    BBTable table = {};
    for (auto sq = 0; sq < N_SQUARES; ++sq) {
        table[sq] = func(sq);
    }
    return table;
}

static constexpr BBMatrix createBBMatrix(auto func) {
    BBMatrix table = {};
    for (auto sq1 = 0; sq1 < N_SQUARES; ++sq1) {
        for (int sq2 = 0; sq2 < N_SQUARES; ++sq2) {
            table[sq1][sq2] = func(sq1, sq2);
        }
    }
    return table;
}

template <size_t N>
static constexpr auto makeCalcAttacks(const int (&moves)[N][2]) {
    return [moves](int sq) {
        int r = sq / 8, f = sq % 8;
        U64 mask = 0;
        for (auto& move : moves) {
            int nr = r + move[0];
            int nf = f + move[1];
            if (0 <= nf && nf < 8 && 0 <= nr && nr < 8) mask |= 1ULL << (nr * 8 + nf);
        }
        return mask;
    };
}

static constexpr auto calcDistance = [](int sq1, int sq2) -> int {
    int r1 = sq1 / 8, f1 = sq1 % 8;
    int r2 = sq2 / 8, f2 = sq2 % 8;
    return std::max(abs(r1 - r2), abs(f1 - f2));
};

static constexpr auto collinearHelper = [](int sq1, int sq2, int r_delta, int f_delta) -> U64 {
    int r = sq1 / 8, f = sq1 % 8;
    U64 mask = 0;
    while (0 <= r && r < 8 && 0 <= f && f < 8) {
        mask |= (1ULL << (r * 8 + f));
        r    += r_delta;
        f    += f_delta;
    }
    return mask;
};

static constexpr auto calcCollinearMask = [](int sq1, int sq2) -> U64 {
    int r1 = sq1 / 8, f1 = sq1 % 8;
    int r2 = sq2 / 8, f2 = sq2 % 8;

    U64 mask = 0;
    if (r1 == r2) {
        mask = collinearHelper(sq1, sq2, 0, 1) | collinearHelper(sq1, sq2, 0, -1);
    } else if (f1 == f2) {
        mask = collinearHelper(sq1, sq2, 1, 0) | collinearHelper(sq1, sq2, -1, 0);
    } else if ((r1 - r2) == (f1 - f2)) {
        // ↙↗ diagonal
        mask = collinearHelper(sq1, sq2, 1, 1) | collinearHelper(sq1, sq2, -1, -1);
    } else if ((r1 + f1) == (r2 + f2)) {
        // ↖↘ diagonal
        mask = collinearHelper(sq1, sq2, 1, -1) | collinearHelper(sq1, sq2, -1, 1);
    }
    return mask;
};

static constexpr auto betweenHelper = [](int sq1, int sq2, int delta) {
    int min_sq = std::min(sq1, sq2);
    int max_sq = std::max(sq1, sq2);
    U64 mask   = 0;
    for (int s = (min_sq + delta); s < max_sq; s += delta) {
        mask |= (1ULL << s);
    }
    return mask;
};

static constexpr auto calcBetweenMask = [](int sq1, int sq2) -> U64 {
    int r1 = sq1 / 8, f1 = sq1 % 8;
    int r2 = sq2 / 8, f2 = sq2 % 8;
    U64 mask = 0;
    if (r1 == r2) {
        mask = betweenHelper(sq1, sq2, 1);
    } else if (f1 == f2) {
        mask = betweenHelper(sq1, sq2, 8);
    } else if ((r1 - r2) == (f1 - f2)) {
        mask = betweenHelper(sq1, sq2, 9);  // ↙↗ diagonal
    } else if ((r1 + f1) == (r2 + f2)) {
        mask = betweenHelper(sq1, sq2, 7);  // ↖↘ diagonal
    }
    return mask;
};

}  // namespace BBUtils
