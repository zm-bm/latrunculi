#pragma once

#include "constants.hpp"
#include "score.hpp"

struct EvalConstants {
    static constexpr Score IsoPawn               = {-5, -15};
    static constexpr Score BackwardPawn          = {-10, -25};
    static constexpr Score DoubledPawn           = {-10, -50};
    static constexpr Score ReachableOutpost      = {30, 20};
    static constexpr Score BishopOutpost         = {30, 20};
    static constexpr Score KnightOutpost         = {50, 30};
    static constexpr Score MinorPawnShield       = {20, 5};
    static constexpr Score BishopLongDiagonal    = {40, 0};
    static constexpr Score BishopPair            = {50, 80};
    static constexpr Score BishopBlockedByPawn   = {-2, -6};
    static constexpr Score RookClosedFile        = {-10, -5};
    static constexpr Score KingZoneXrayAttack    = {20, 0};
    static constexpr Score QueenDiscoveredAttack = {-50, -25};

    // Bonus for rook on open files: [0] = semi-open, [1] = fully open
    static constexpr Score RookOpenFile[] = {{20, 10}, {40, 20}};

    // Bonus for friendly pawn by rank (index = pawn rank; 0 = no pawn)
    static constexpr Score PawnRankShelter[] = {
        {-30, 0}, {60, 0}, {35, 0}, {-20, 0}, {-5, 0}, {-20, 0}, {-80, 0}};

    // Pawn storm penalty by pawn rank:
    // PawnRankStorm[unblocked=0 / blocked=1][pawn rank (0 = no pawn)]
    static constexpr Score PawnRankStorm[][N_PIECES] = {
        {{0, 0}, {-20, 0}, {-120, 0}, {-60, 0}, {-45, 0}, {-20, 0}, {-10, 0}},
        {{0, 0}, {0, 0}, {-60, -60}, {0, -20}, {5, -15}, {10, -10}, {15, -5}}};

    // Score for king based on file openness: [friendly file open][enemy file open]
    // 0 = closed file, 1 = open file
    static constexpr Score KingOpenFile[][2] = {
        {{20, -10}, {10, 5}},
        {{0, 0}, {-10, 5}},
    };

    // Score for king based on file (index = king file)
    static constexpr Score KingFile[] = {
        {20, 0}, {5, 0}, {-15, 0}, {-30, 0}, {-30, 0}, {-15, 0}, {5, 0}, {20, 0}};

    // Score for potentially hanging piece
    static constexpr Score WeakPiece[] = {
        ZERO_SCORE, ZERO_SCORE, {-20, -10}, {-25, -15}, {-50, -25}, {-100, -50}};

    // clang-format off

    // Knight mobility score (index = number of legal moves)
    static constexpr Score KnightMobility[] = {
        {-40, -48}, {-32, -36}, {-8, -20}, {-2, -12}, {2, 6},
        {8, 8},     {12, 12},   {16, 16},  {24, 16}
    };

    // Bishop mobility score (index = number of legal moves)
    static constexpr Score BishopMobility[] = {
        {-32, -40}, {-16, -16}, {8, -4},   {16, 8},   {24, 16},
        {32, 24},   {32, 36},   {40, 36},  {40, 40},  {44, 48},
        {48, 48},   {56, 56},   {56, 56},  {64, 64}
    };

    // Rook mobility score (index = number of legal moves)
    static constexpr Score RookMobility[] = {
        {-40, -56}, {-16, -8}, {0, 12},   {0, 28},   {4, 44},
        {8, 64},    {12, 64},  {20, 80},  {28, 88},  {28, 88},
        {28, 96},   {32, 104}, {36, 108}, {40, 112}, {44, 120}
    };

    // Queen mobility score (index = number of legal moves)
    static constexpr Score QueenMobility[] = {
        {-20, -32}, {-12, -20}, {-4, -4},   {-4, 12},   {12, 24},  {16, 36},  {16, 40},
        {24, 48},   {28, 48},   {36, 60},   {40, 60},   {44, 64},  {44, 80},  {48, 80},
        {48, 88},   {48, 88},   {48, 88},   {48, 92},   {52, 96},  {56, 96},  {60, 100},
        {68, 108},  {68, 112},  {68, 112},  {72, 116},  {72, 120}, {76, 124}, {80, 140}
    };
    // clang-format on

    // Mobility score lookup by piece type. nullptr for pieces without mobility scoring
    static constexpr const Score* Mobility[] = {
        nullptr,
        nullptr,
        KnightMobility,
        BishopMobility,
        RookMobility,
        QueenMobility,
    };
};