#ifndef LATRUNCULI_EVAL_H
#define LATRUNCULI_EVAL_H

#include <array>

#include "bb.hpp"
#include "constants.hpp"
#include "score.hpp"
#include "types.hpp"

namespace Eval {

inline int pieceValue(Phase ph, Color c, PieceType pt) { return PIECE_VALUES[ph][c][pt]; }

inline int psqValue(Phase ph, Color c, PieceType pt, Square sq) {
    int score = PSQ_VALUES[pt - 1][ph][SQUARE_MAP[c][sq]];
    return (2 * c * score) - score;
}

template <Color c>
inline U64 passedPawns(U64 pawns, U64 enemyPawns) {
    constexpr Color enemy = ~c;
    return pawns & ~BB::getAllFrontSpan<enemy>(enemyPawns);
}

inline U64 isolatedPawns(U64 pawns) {
    U64 pawnsFill = BB::fillFiles(pawns);
    return (pawns & ~BB::shiftWest(pawnsFill)) & (pawns & ~BB::shiftEast(pawnsFill));
}

template <Color c>
inline U64 backwardsPawns(U64 pawns, U64 enemyPawns) {
    constexpr Color enemy = ~c;
    U64 stops = BB::movesByPawns<PawnMove::PUSH, c>(pawns);
    U64 attackSpan = BB::getFrontAttackSpan<c>(pawns);
    U64 enemyAttacks = BB::attacksByPawns<enemy>(enemyPawns);
    return BB::movesByPawns<PawnMove::PUSH, enemy>(stops & enemyAttacks & ~attackSpan);
}

template <Color c>
inline U64 doubledPawns(U64 pawns) {
    // pawns that have friendly pawns behind them that aren't supported
    U64 pawnsBehind = pawns & BB::spanFront<c>(pawns);
    U64 supported = BB::attacksByPawns<c>(pawns);
    return pawnsBehind & ~supported;
}

template <Color c>
inline U64 blockedPawns(U64 pawns, U64 enemyPawns) {
    constexpr Color enemy = ~c;
    return pawns & BB::movesByPawns<PawnMove::PUSH, enemy>(enemyPawns);
}

template <Color c>
inline U64 outpostSquares(U64 pawns, U64 enemyPawns) {
    constexpr U64 mask = (c == WHITE) ? WHITE_OUTPOSTS : BLACK_OUTPOSTS;
    constexpr Color enemy = ~c;
    U64 holes = ~BB::getFrontAttackSpan<enemy>(enemyPawns) & mask;
    return holes & BB::attacksByPawns<c>(pawns);
}

/**
 * @brief Evaluates the alignment of pawns and bishops on the board along with the influence of
 * blocked central pawns.
 *
 * This function calculates a score based on the alignment of the current player's pawns and
 * bishops. If there are pawns on the same color squares as the bishops, they can hinder the
 * bishop's movement. The score is further adjusted by the number of blocked pawns in the central
 * files (C, D, E, F).
 *
 * @tparam c The color of the bishops (WHITE or BLACK).
 * @param bishops A bitboard representing the positions of the bishops.
 * @param pawns A bitboard representing the positions of the pawns.
 * @param enemyPawns A bitboard representing the positions of the opponent's pawns.
 * @return An integer score derived from the alignment of friendly pawns and bishops
 *         adjusted by the number of blocked central pawns.
 */
template <Color c>
inline int bishopPawnBlockers(U64 bishops, U64 pawns, U64 enemyPawns) {
    // Calculate the attack coverage of pawns for the current player using a bitboard.
    U64 pawnAttacks = BB::attacksByPawns<c>(pawns);

    // Determine the number of blocked pawns on central files.
    int blocked = BB::bitCount(blockedPawns<c>(pawns, enemyPawns) & CENTER_FILES);

    // Separate bishops into light-squared and dark-squared groups.
    U64 lightBishops = bishops & LIGHT_SQUARES;
    U64 darkBishops = bishops & DARK_SQUARES;

    // Count pawns on light squares and dark squares.
    int lightPawnCount = BB::bitCount(pawns & LIGHT_SQUARES);
    int darkPawnCount = BB::bitCount(pawns & DARK_SQUARES);

    // Calculate the factor for score adjustment, influenced by the presence
    // of pawn attacks on the same color bishop's path and blocked central pawns.
    int lightPawnFactor = (pawnAttacks & lightBishops ? 0 : 1) + blocked;
    int darkPawnFactor = (pawnAttacks & darkBishops ? 0 : 1) + blocked;

    // Compute scores for light and dark bishops based on their interaction with pawns.
    int lightScore = BB::bitCount(lightBishops) * lightPawnCount * lightPawnFactor;
    int darkScore = BB::bitCount(darkBishops) * darkPawnCount * darkPawnFactor;

    // Return the total score, accounting for both light and dark bishop interactions.
    return lightScore + darkScore;
}

}  // namespace Eval

// template <bool debug>
// int Chess::evalDeprecated() const
// {
//     // Give bonuses to queens/rooks on open/half open files
//     U64 openFiles = ~BB::fillFiles(allPawns),
//         wHalfOpenFiles = ~BB::fillFiles(wPawns) ^ openFiles,
//         bHalfOpenFiles = ~BB::fillFiles(bPawns) ^ openFiles,
//         wSliders = board.straightSliders(WHITE),
//         bSliders = board.straightSliders(BLACK);
//     double openFileValue = (opPhase * Eval::OpenFileBonus[MIDGAME]
//                           + egPhase * Eval::OpenFileBonus[ENDGAME]) / TOTALPHASE;
//     double openFileScore = (BB::bitCount(wSliders & openFiles) - BB::bitCount(bSliders &
//     openFiles)) * openFileValue; double halfOpenFileValue = (opPhase *
//     Eval::HalfOpenFileBonus[MIDGAME]
//                               + egPhase * Eval::HalfOpenFileBonus[ENDGAME]) / TOTALPHASE;
//     double halfOpenFileScore = (BB::bitCount(wSliders & wHalfOpenFiles) - BB::bitCount(bSliders &
//     bHalfOpenFiles)) * halfOpenFileValue;

//     // As pawns are captured, penalize knights and give bonus to rooks
//     int capturedPawns = 16 - BB::bitCount(allPawns);
//     int knightPen = (board.count<KNIGHT>(WHITE) - board.count<KNIGHT>(BLACK))
//                   * (capturedPawns * Eval::KNIGHT_PENALTY_PER_PAWN);
//     int rookBonus = (board.count<ROOK>(WHITE) - board.count<ROOK>(BLACK))
//                   * (capturedPawns * Eval::ROOK_BONUS_PER_PAWN);

//     /**
//      *  Evaluate individual pieces
//      */
//     Square wking = board.getKingSq(WHITE),
//            bking = board.getKingSq(BLACK);
//     U64 occ = board.occupancy(),
//         pieces = 0,
//         wHoles = ~BB::getFrontAttackSpan<WHITE>(wPawns) & BB::WHITEHOLES,
//         bHoles = ~BB::getFrontAttackSpan<BLACK>(bPawns) & BB::BLACKHOLES,
//         wOutposts = bHoles & BB::attacksByPawns<WHITE>(wPawns),
//         bOutposts = wHoles & BB::attacksByPawns<BLACK>(bPawns),
//         allOutposts = wOutposts | bOutposts;

//     // Knights
//     double wKnightScore = 0;
//     pieces = board.getPieces<KNIGHT>(WHITE);
//     while (pieces)
//     {
//         Square sq = BB::advanced<WHITE>(pieces);
//         pieces &= BB::clear(sq);
//         wKnightScore += Eval::KNIGHT_TROPISM[BB::DISTANCE[bking][sq]];
//         if (Defs::rankFromSq(sq) == RANK1)
//             wKnightScore += Eval::BACK_RANK_MINOR_PENALTY * openingModifier;
//         U64 validOutposts = BB::movesByPiece<KNIGHT>(sq, occ) | BB::set(sq);
//         if (validOutposts & allOutposts)
//             wKnightScore += Eval::MINOR_OUTPOST_BONUS;
//     }
//     double bKnightScore = 0;
//     pieces = board.getPieces<KNIGHT>(BLACK);
//     while (pieces)
//     {
//         Square sq = BB::advanced<BLACK>(pieces);
//         pieces &= BB::clear(sq);
//         bKnightScore += Eval::KNIGHT_TROPISM[BB::DISTANCE[wking][sq]];
//         if (Defs::rankFromSq(sq) == RANK8)
//             bKnightScore += Eval::BACK_RANK_MINOR_PENALTY * openingModifier;
//         U64 validOutposts = BB::movesByPiece<KNIGHT>(sq, occ) | BB::set(sq);
//         if (validOutposts & allOutposts)
//             bKnightScore += Eval::MINOR_OUTPOST_BONUS;
//     }
//     if (debug)
// 	{
// 		std::cout << " W Knights  |      -      |      -      | "
//                   << std::setw(6) << wKnightScore << std::endl
//                   << " B Knights  |      -      |      -      | "
//                   << std::setw(6) << -bKnightScore << std::endl;
// 	}

//     // Bishops
//     double wBishopScore = 0;
//     pieces = board.getPieces<BISHOP>(WHITE);
//     if ((pieces & BB::WHITE_SQUARES) && (pieces & BB::BLACK_SQUARES))
//         wBishopScore += (opPhase * Eval::BishopPairBonus[MIDGAME]
//                       + (egPhase * Eval::BishopPairBonus[ENDGAME])) / TOTALPHASE;
//     while (pieces)
//     {
//         Square sq = BB::advanced<WHITE>(pieces);
//         pieces &= BB::clear(sq);
//         wBishopScore += Eval::BISHOP_TROPISM[BB::DISTANCE[bking][sq]];
//         if (Defs::rankFromSq(sq) == RANK1)
//             wBishopScore += Eval::BACK_RANK_MINOR_PENALTY * openingModifier;
//         U64 validOutposts = BB::movesByPiece<BISHOP>(sq, occ) | BB::set(sq);
//         if (validOutposts & allOutposts)
//             wBishopScore += Eval::MINOR_OUTPOST_BONUS;
//     }
//     double bBishopScore = 0;
//     pieces = board.getPieces<BISHOP>(BLACK);
//     if ((pieces & BB::WHITE_SQUARES) && (pieces & BB::BLACK_SQUARES))
//         bBishopScore += (opPhase * Eval::BishopPairBonus[MIDGAME]
//                       + (egPhase * Eval::BishopPairBonus[ENDGAME])) / TOTALPHASE;
//     while (pieces)
//     {
//         Square sq = BB::advanced<BLACK>(pieces);
//         pieces &= BB::clear(sq);
//         bBishopScore += Eval::BISHOP_TROPISM[BB::DISTANCE[wking][sq]];
//         if (Defs::rankFromSq(sq) == RANK8)
//             bBishopScore += Eval::BACK_RANK_MINOR_PENALTY * openingModifier;
//         U64 validOutposts = BB::movesByPiece<BISHOP>(sq, occ) | BB::set(sq);
//         if (validOutposts & allOutposts)
//             bBishopScore += Eval::MINOR_OUTPOST_BONUS;
//     }
//     if (debug)
// 	{
// 		std::cout << " W Bishops  |      -      |      -      | "
//                   << std::setw(6) << wBishopScore << std::endl
//                   << " B Bishops  |      -      |      -      | "
//                   << std::setw(6) << -bBishopScore << std::endl;
// 	}

//     // Rooks
//     double wRookScore = 0;
//     pieces = board.getPieces<ROOK>(WHITE);
//     while (pieces)
//     {
//         Square sq = BB::advanced<WHITE>(pieces);
//         pieces &= BB::clear(sq);
//         wRookScore += Eval::ROOK_TROPISM[BB::DISTANCE[bking][sq]];
//         wRookScore += Eval::ROOK_TROPISM[BB::DISTANCE[wking][sq]] * openingModifier;
//         if (Defs::rankFromSq(sq) >= RANK7)
//             wRookScore += Eval::ROOK_ON_SEVENTH_BONUS * openingModifier;
//         if (BB::movesByPiece<ROOK>(sq, occ) & pieces)
//             wRookScore += Eval::CONNECTED_ROOK_BONUS;
//     }
//     double bRookScore = 0;
//     pieces = board.getPieces<ROOK>(BLACK);
//     while (pieces)
//     {
//         Square sq = BB::advanced<BLACK>(pieces);
//         pieces &= BB::clear(sq);
//         bRookScore += Eval::ROOK_TROPISM[BB::DISTANCE[wking][sq]];
//         bRookScore += Eval::ROOK_TROPISM[BB::DISTANCE[bking][sq]] * openingModifier;
//         if (Defs::rankFromSq(sq) <= RANK2)
//             bRookScore += Eval::ROOK_ON_SEVENTH_BONUS * openingModifier;
//         if (BB::movesByPiece<ROOK>(sq, occ) & pieces)
//             bRookScore += Eval::CONNECTED_ROOK_BONUS;
//     }
//     if (debug)
// 	{
// 		std::cout << " W Rooks    |      -      |      -      | "
//                   << std::setw(6) << wRookScore << std::endl
//                   << " B Rooks    |      -      |      -      | "
//                   << std::setw(6) << -bRookScore << std::endl;
// 	}

//     // Queens
//     double wQueenScore = 0;
//     pieces = board.getPieces<QUEEN>(WHITE);
//     while (pieces)
//     {
//         Square sq = BB::advanced<WHITE>(pieces);
//         pieces &= BB::clear(sq);
//         wQueenScore += Eval::QUEEN_TROPISM[BB::DISTANCE[bking][sq]];
//         wQueenScore += Eval::QUEEN_TROPISM[BB::DISTANCE[wking][sq]] * openingModifier;
//     }
//     double bQueenScore = 0;
//     pieces = board.getPieces<QUEEN>(BLACK);
//     while (pieces)
//     {
//         Square sq = BB::advanced<BLACK>(pieces);
//         pieces &= BB::clear(sq);
//         bQueenScore += Eval::QUEEN_TROPISM[BB::DISTANCE[wking][sq]];
//         bQueenScore += Eval::QUEEN_TROPISM[BB::DISTANCE[bking][sq]] * openingModifier;
//     }
//     if (debug)
// 	{
// 		std::cout << " W Queens   |      -      |      -      | "
//                   << std::setw(6) << wQueenScore << std::endl
//                   << " B Queens   |      -      |      -      | "
//                   << std::setw(6) << -bQueenScore << std::endl;
// 	}

//     double pieceScore = (wKnightScore - bKnightScore)
//                       + (wBishopScore - bBishopScore)
//                       + (wRookScore - bRookScore)
//                       + (wQueenScore - bQueenScore)
//                       + openFileScore + halfOpenFileScore + knightPen + rookBonus;
//     if (debug)
// 	{
// 		std::cout << " Open files |      -      |      -      | "
//                   << std::setw(6) << openFileScore + halfOpenFileScore << std::endl
//                   << " Pawns adj. |      -      |      -      | "
//                   << std::setw(6) << knightPen + rookBonus << std::endl;
// 	}

//     // Kings
//     U64 wShield = BB::kingShield<WHITE>(wking),
//         bShield = BB::kingShield<BLACK>(bking),
//         wShieldStrong = wShield & wPawns,
//         bShieldStrong = bShield & bPawns,
//         wShieldWeak = BB::shiftNorth(wShield) & wPawns,
//         bShieldWeak = BB::shiftSouth(bShield) & bPawns;
//     double rawKingScore = Eval::STRONG_KING_SHIELD_BONUS * (BB::bitCount(wShieldStrong) -
//     BB::bitCount(bShieldStrong))
//                         + Eval::WEAK_KING_SHIELD_BONUS * (BB::bitCount(wShieldWeak) -
//                         BB::bitCount(bShieldWeak));
//     double kingScore = rawKingScore * std::min(16, (int)moveCounter) / 16 * openingModifier;
//     if (debug)
// 	{
// 		std::cout << " King Safety| "
//                   << std::setw(6) << kingScore
//                   << "      |      -      | "
//                   << std::setw(6) << kingScore << std::endl;
// 	}

//     // double mobilityScore = board.calculateMobilityScore(opPhase, egPhase) / TOTALPHASE;
//     double mobilityScore = 0;
//     int color = 2*turn - 1; // +1 when turn=WHITE, -1 when turn=BLACK
//     double score = color * (materialScore + positionScore + pawnScore +
//                          pieceScore + kingScore + mobilityScore);

//     if (debug)
// 	{
// 		std::cout << " Mobility   |      -      |      -      | "
//                   << std::setw(6) << mobilityScore << std::endl
//                   << "------------+-------------+-------------+----------" << std::endl
//                   << " Sub Total  |      -      |      -      | "
//                   << std::setw(6) << score << std::endl
//                   << " Tempo      |      -      |      -      | "
//                   << std::setw(6) << color * TEMPO_BONUS << std::endl
//                   << " Total      |      -      |      -      | "
//                   << std::setw(6) << score + TEMPO_BONUS << std::endl;
// 	}

//     return score + TEMPO_BONUS;
// }

// int calculateMobilityScore(const int, const int) const;
// template <PieceType>
// int calculateMobilityScore(const int, const int) const;
// template <PieceType p>
// int calculateMobility(U64 bitboard, U64 targets, U64 occ) const;

// int Board::calculateMobilityScore(const int opPhase, const int egPhase) const
// {
//     int score = 0;
//     score += calculateMobilityScore<KNIGHT>(opPhase, egPhase);
//     score += calculateMobilityScore<BISHOP>(opPhase, egPhase);
//     score += calculateMobilityScore<ROOK  >(opPhase, egPhase);
//     score += calculateMobilityScore<QUEEN >(opPhase, egPhase);
//     return score;
// }

// template<PieceType p>
// int Board::calculateMobilityScore(const int opPhase, const int egPhase) const
// {
//     int mobilityScore = 0;
//     U64 occ = occupancy();

//     int whitemoves = calculateMobility<p>(getPieces<p>(WHITE),
//                                           ~getPieces<ALL_PIECE_TYPES>(WHITE), occ);
//     mobilityScore += whitemoves * (opPhase * Eval::MobilityScaling[MIDGAME][p]
//                                  + egPhase * Eval::MobilityScaling[ENDGAME][p]);

//     int blackmoves = calculateMobility<p>(getPieces<p>(BLACK),
//                                           ~getPieces<ALL_PIECE_TYPES>(BLACK), occ);
//     mobilityScore -= blackmoves * (opPhase * Eval::MobilityScaling[MIDGAME][p]
//                                  + egPhase * Eval::MobilityScaling[ENDGAME][p]);
//     return mobilityScore;
// }

// template <PieceType p>
// int Board::calculateMobility(U64 bitboard, U64 targets, U64 occ) const {
//     int nmoves = 0;

//     while (bitboard) {
//         // Pop lsb bit and clear it from the bitboard
//         Square from = BB::lsb(bitboard);
//         bitboard &= BB::clear(from);

//         U64 mobility = BB::movesByPiece<p>(from, occ) & targets;
//         nmoves += BB::bitCount(mobility);
//     }

//     return nmoves;
// }

#endif
