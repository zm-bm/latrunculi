#ifndef ANTONIUS_GLOBAL_H
#define ANTONIUS_GLOBAL_H

#include <string>
#include "types.hpp"

namespace G {

    extern const std::string STARTFEN;
    extern const std::string KIWIPETE;
    extern const std::string TESTFEN1;
    extern const std::string TESTFEN2;
    extern const std::string TESTFEN3;
    extern const std::string TESTFEN4;
    extern const std::string TESTFEN5;

    extern U64 KNIGHT_ATTACKS[];
    extern U64 KING_ATTACKS[];
    extern U64 SQUARE_BB[];
    extern U64 CLEAR_BB[];
    extern U64 IN_BETWEEN[][64];
    extern U64 LINE_BB[][64];
    extern int DISTANCE[][64];

    extern const U64 WHITESQUARES;
    extern const U64 BLACKSQUARES;
    extern const U64 WHITEHOLES;
    extern const U64 BLACKHOLES;

    extern const Rank RANK[][8];
    extern const File FILE[][8];
    extern const U64 RANK_MASK[];
    extern const U64 FILE_MASK[];

    inline U64 bitset(Square sq)
    {
        return SQUARE_BB[sq];
    }

    inline U64 bitclear(Square sq)
    {
        return CLEAR_BB[sq];
    }

    inline U64 rankmask(Rank r, Color c) {
        return RANK_MASK[RANK[c][r]];
    }

    inline U64 filemask(File f, Color c) {
        return FILE_MASK[FILE[c][f]];
    }

    void initBitset();
    void init();
    std::vector<std::string> split(const std::string&, char);

}

#endif