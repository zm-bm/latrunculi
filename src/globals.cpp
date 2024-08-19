#include "globals.hpp"
#include <string>
#include <sstream>
#include <algorithm>
#include "types.hpp"
#include "magics.hpp"
#include "zobrist.hpp"

namespace G
{

    const std::string STARTFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const std::string KIWIPETE = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    const std::string TESTFEN1 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
    const std::string TESTFEN2 = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
    const std::string TESTFEN3 = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1";
    const std::string TESTFEN4 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
    const std::string TESTFEN5 = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";

    U64 KNIGHT_ATTACKS[64];
    U64 KING_ATTACKS[64];
    U64 SQUARE_BB[64];
    U64 CLEAR_BB[64];
    U64 IN_BETWEEN[64][64];
    U64 LINE_BB[64][64];
    int DISTANCE[64][64];

    const U64 WHITESQUARES = 0x55AA55AA55AA55AA;
    const U64 BLACKSQUARES = 0xAA55AA55AA55AA55;
    const U64 WHITEHOLES   = 0x0000003CFFFF0000;
    const U64 BLACKHOLES   = 0x0000FFFF3C000000;

    const Rank RANK[2][8] = {
        { RANK8, RANK7, RANK6, RANK5, RANK4, RANK3, RANK2, RANK1 },
        { RANK1, RANK2, RANK3, RANK4, RANK5, RANK6, RANK7, RANK8 }
    };

    const File FILE[2][8] = {
        { FILE8, FILE7, FILE6, FILE5, FILE4, FILE3, FILE2, FILE1 },
        { FILE1, FILE2, FILE3, FILE4, FILE5, FILE6, FILE7, FILE8 }
    };

    const U64 RANK_MASK[8] = {
        0xFF,
        0xFF00,
        0xFF0000,
        0xFF000000,
        0xFF00000000,
        0xFF0000000000,
        0xFF000000000000,
        0xFF00000000000000
    };

    const U64 FILE_MASK[8] = {
        0x0101010101010101,
        0x202020202020202,
        0x404040404040404,
        0x808080808080808,
        0x1010101010101010,
        0x2020202020202020,
        0x4040404040404040,
        0x8080808080808080
    };

}

void G::init()
{
    // Initialize magics and Zobrist keys
    Magic::init();
    Zobrist::init();

    // Initialize bit set and clear masks
    for (Square sq = A1; sq != INVALID; sq++)
    {
        SQUARE_BB[sq] = 0x1ull << sq;
        CLEAR_BB[sq] = ~SQUARE_BB[sq];
    }

    // Initialize knight and king attack masks
    for (Square sq = A1; sq != INVALID; sq++)
    {
        auto file = Types::getFile(sq);
        auto rank = Types::getRank(sq);

        auto target_file = file - 1;
        auto target_rank = rank + 2;
        auto square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KNIGHT_ATTACKS[sq] |= bitset(square);

        target_file = file + 1;
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KNIGHT_ATTACKS[sq] |= bitset(square);

        target_file = file - 2; 
        target_rank = rank + 1;
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KNIGHT_ATTACKS[sq] |= bitset(square);

        target_file = file + 2;
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KNIGHT_ATTACKS[sq] |= bitset(square);

        target_rank = rank - 1;
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KNIGHT_ATTACKS[sq] |= bitset(square);

        target_file = file - 2;
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KNIGHT_ATTACKS[sq] |= bitset(square);

        target_file = file - 1; 
        target_rank = rank - 2;
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KNIGHT_ATTACKS[sq] |= bitset(square);

        target_file = file + 1;
        target_rank = rank - 2;
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KNIGHT_ATTACKS[sq] |= bitset(square);

        target_rank = rank + 1;
        target_file = file + 1;
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KING_ATTACKS[sq] |= bitset(square);

        target_file = file;
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KING_ATTACKS[sq] |= bitset(square);

        target_file = file - 1; 
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KING_ATTACKS[sq] |= bitset(square);

        target_rank = rank;
        target_file = file + 1;
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KING_ATTACKS[sq] |= bitset(square);

        target_file = file - 1;
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KING_ATTACKS[sq] |= bitset(square);

        target_rank = rank - 1;
        target_file = file + 1; 
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KING_ATTACKS[sq] |= bitset(square);

        target_file = file;
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KING_ATTACKS[sq] |= bitset(square);

        target_file = file - 1;
        square = Types::getSquare(target_file, target_rank);
        if (Types::validFile(target_file) && Types::validRank(target_rank))
            KING_ATTACKS[sq] |= bitset(square);
    }

    // Initialize other bit masks
    for (Square sq1 = A1; sq1 != INVALID; sq1++)
    {
        for (Square sq2 = A1; sq2 != INVALID; sq2++)
        {
            int rankDistance = abs(Types::getRank(sq1) - Types::getRank(sq2));
            int fileDistance = abs(Types::getFile(sq1) - Types::getFile(sq2));
            DISTANCE[sq1][sq2] = std::max(rankDistance, fileDistance);

            if (Magic::getBishopAttacks(sq1, 0) & bitset(sq2))
            {
                IN_BETWEEN[sq1][sq2] = Magic::getBishopAttacks(sq1, bitset(sq2)) \
                                        & Magic::getBishopAttacks(sq2, bitset(sq1));
                LINE_BB[sq1][sq2] = (Magic::getBishopAttacks(sq1, 0) & Magic::getBishopAttacks(sq2, 0))
                                    | bitset(sq1) | bitset(sq2);
            }

            if (Magic::getRookAttacks(sq1, 0) & bitset(sq2))
            {
                IN_BETWEEN[sq1][sq2] = Magic::getRookAttacks(sq1, bitset(sq2)) \
                                        & Magic::getRookAttacks(sq2, bitset(sq1));
                LINE_BB[sq1][sq2] = (Magic::getRookAttacks(sq1, 0) & Magic::getRookAttacks(sq2, 0))
                                    | bitset(sq1) | bitset(sq2);
            }
        }
    }
}

std::vector<std::string> G::split(const std::string &s, char delim)
{
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(s);

    while (std::getline(ss, token, delim)) {
        tokens.push_back(token);
    }

    return tokens;
}
