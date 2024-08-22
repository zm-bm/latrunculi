#ifndef ANTONIUS_GLOBAL_H
#define ANTONIUS_GLOBAL_H

#include <string>
#include <array>
#include "types.hpp"

namespace G {
    // default fen / position
    const std::string STARTFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    // base bitboards
    constexpr std::array<U64, NSQUARES> generateBitset() {
        std::array<U64, NSQUARES> arr = {};
        for (int i = 0; i < NSQUARES; ++i) {
            arr[i] = 1ull << i;
        }
        return arr;
    }
    constexpr std::array<U64, NSQUARES> generateBitclear() {
        std::array<U64, NSQUARES> arr = {};
        for (int i = 0; i < NSQUARES; ++i) {
            arr[i] = ~(1ull << i);
        }
        return arr;
    }
    constexpr std::array<U64, NSQUARES> BITSET = generateBitset();
    constexpr std::array<U64, NSQUARES> BITCLEAR = generateBitclear();

    // 2d bitboards
    constexpr int calculateDistance(Square sq1, Square sq2) {
        int rankDistance = abs(Types::getRank(sq1) - Types::getRank(sq2));
        int fileDistance = abs(Types::getFile(sq1) - Types::getFile(sq2));
        return std::max(rankDistance, fileDistance);
    }
    constexpr std::array<std::array<int, NSQUARES>, NSQUARES> generateDistance() {
        std::array<std::array<int, NSQUARES>, NSQUARES> table = {};

        for (int i = 0; i < NSQUARES; ++i) {
            for (int j = 0; j < NSQUARES; ++j) {
                table[i][j] = calculateDistance(static_cast<Square>(i), static_cast<Square>(j));
            }
        }

        return table;
    }
    constexpr std::array<std::array<int, NSQUARES>, NSQUARES> DISTANCE = generateDistance();

    // constexpr std::array<std::array<U64, NSQUARES>, NSQUARES> generateBitsBetween() {
    //     std::array<std::array<U64, NSQUARES>, NSQUARES> table = {};

    //     for (int i = 0; i < NSQUARES; ++i) {
    //         Square sq1 = static_cast<Square>(i);
    //         for (int j = 0; j < NSQUARES; ++j) {
    //             Square sq2 = static_cast<Square>(j);
    //             if (Magic::getBishopAttacks(sq1, 0) & BITSET[sq2]) {
    //                 table[sq1][sq2] = (
    //                     Magic::getBishopAttacks(sq1, BITSET[sq2])
    //                     & Magic::getBishopAttacks(sq2, BITSET[sq1])
    //                 );
    //             }

    //             if (Magic::getRookAttacks(sq1, 0) & BITSET[sq2]) {
    //                 table[sq1][sq2] = (
    //                     Magic::getRookAttacks(sq1, BITSET[sq2])
    //                     & Magic::getRookAttacks(sq2, BITSET[sq1])
    //                 );
    //             }
    //         }
    //     }

    //     return table;
    // }
    // constexpr std::array<std::array<U64, NSQUARES>, NSQUARES> BITS_BETWEEN = generateBitsBetween();

    extern U64 BITS_BETWEEN[][64];
    extern U64 BITS_INLINE[][64];

    constexpr void addTarget(Square orig, File targetFile, Rank targetRank, std::array<U64, 64>& arr) {
        auto square = Types::getSquare(targetFile, targetRank);
        if (Types::validFile(targetFile) && Types::validRank(targetRank)) {
            arr[orig] |= G::BITSET[square];
        }
    }

    // attack bitboards
    constexpr std::array<U64, 64> generateKnightAttacks() {
        std::array<U64, 64> arr = {};

        for (std::size_t i = 0; i < arr.size(); ++i) {
            Square sq = static_cast<Square>(i);
            auto file = Types::getFile(sq);
            auto rank = Types::getRank(sq);
            addTarget(sq, file + 2, rank + 1, arr);
            addTarget(sq, file + 2, rank - 1, arr);
            addTarget(sq, file - 2, rank + 1, arr);
            addTarget(sq, file - 2, rank - 1, arr);
            addTarget(sq, file + 1, rank + 2, arr);
            addTarget(sq, file - 1, rank + 2, arr);
            addTarget(sq, file + 1, rank - 2, arr);
            addTarget(sq, file - 1, rank - 2, arr);
        }
        return arr;
    }

    constexpr std::array<U64, 64> generateKingAttacks() {
        std::array<U64, 64> arr = {};

        for (std::size_t i = 0; i < arr.size(); ++i) {
            Square sq = static_cast<Square>(i);
            auto file = Types::getFile(sq);
            auto rank = Types::getRank(sq);
            addTarget(sq, file - 1, rank - 1, arr);
            addTarget(sq, file - 1, rank + 1, arr);
            addTarget(sq, file + 1, rank - 1, arr);
            addTarget(sq, file + 1, rank + 1, arr);
            addTarget(sq, file,     rank - 1, arr);
            addTarget(sq, file,     rank + 1, arr);
            addTarget(sq, file - 1, rank,     arr);
            addTarget(sq, file + 1, rank,     arr);
        }
        return arr;
    }

    constexpr std::array<U64, 64> KNIGHT_ATTACKS = generateKnightAttacks();
    constexpr std::array<U64, 64> KING_ATTACKS = generateKingAttacks();

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