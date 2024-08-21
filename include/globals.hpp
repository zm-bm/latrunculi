#ifndef ANTONIUS_GLOBAL_H
#define ANTONIUS_GLOBAL_H

#include <string>
#include "types.hpp"

namespace G {
    // default fen / position
    extern const std::string STARTFEN;

    // base bitboards
    extern U64 BITSET[];
    extern U64 BITCLEAR[];

    // 2d bitboards
    extern U64 BITS_BETWEEN[][64];
    extern U64 BITS_INLINE[][64];
    extern int DISTANCE[][64];

    // attack bitboards
    extern U64 KNIGHT_ATTACKS[];
    extern U64 KING_ATTACKS[];

    extern const Rank RANK[][8];
    extern const File FILE[][8];
    extern const U64 RANK_MASK[];
    extern const U64 FILE_MASK[];

    inline U64 rankmask(Rank r, Color c) {
        return RANK_MASK[RANK[c][r]];
    }

    inline U64 filemask(File f, Color c) {
        return FILE_MASK[FILE[c][f]];
    }

    void initBitboards();
    void init();
    std::vector<std::string> split(const std::string&, char);
}

#endif