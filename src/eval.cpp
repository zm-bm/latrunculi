#include "chess.hpp"
#include <iomanip>
#include "eval.hpp"


// template <bool debug>
// int Chess::evalDeprecated() const
// {
//     double totalphase = static_cast<double>(TOTALPHASE);

//     // Taper the eval between opening and endgame values
//     int opPhase = calculatePhase();
//     int egPhase = (TOTALPHASE - opPhase);
//     double openingModifier = opPhase / totalphase;

//     // Determine position score
//     double opening = openingScore * opPhase;
//     double endgame = endgameScore * egPhase;
//     double positionScore = (opening + endgame) / totalphase;

//     if (debug)
// 	{
// 		std::cout << " Term       |   Opening   |   Endgame   |   Total   " << std::endl
// 			      << "------------+-------------+-------------+-----------" << std::endl
//                   << " Material   |      -      |      -      | "
//                   << std::setw(6) << materialScore << std::endl
//                   << " Piece Sq   | " 
//                   << std::setw(6) << opening / totalphase << std::setw(8) << " | "
//                   << std::setw(6) << endgame / totalphase << std::setw(8) << " | "
//                   << std::setw(6) << positionScore << std::endl;
// 	}

//     // Evaluate pawn structure
//     U64 wPawns = board.getPieces<PAWN>(WHITE),
//         bPawns = board.getPieces<PAWN>(BLACK),
//         allPawns = wPawns | bPawns;

//     // Passed pawns
//     U64 wPassedPawns = wPawns & ~BB::getAllFrontSpan<BLACK>(bPawns),
//         bPassedPawns = bPawns & ~BB::getAllFrontSpan<WHITE>(wPawns);
//     double passedPawnValue = (opPhase * Eval::PassedPawnBonus[MIDGAME]
//                             + egPhase * Eval::PassedPawnBonus[ENDGAME]) / TOTALPHASE;
//     double passedPawnScore = (BB::bitCount(wPassedPawns) - BB::bitCount(bPassedPawns)) * passedPawnValue;

//     // Doubled+tripled pawns
//     U64 wPawnsAhead  = wPawns & ~BB::spanFront<WHITE>(wPawns),
//         wPawnsBehind = wPawns & ~BB::spanBack<WHITE>(wPawns),
//         wTriplePawns = wPawnsAhead & wPawnsBehind,
//         wDoublePawns = (wPawnsAhead | wPawnsBehind) ^ wTriplePawns,
//         bPawnsAhead  = bPawns & ~BB::spanFront<BLACK>(bPawns),
//         bPawnsBehind = bPawns & ~BB::spanBack<BLACK>(bPawns),
//         bTriplePawns = bPawnsAhead & bPawnsBehind,
//         bDoublePawns = (bPawnsAhead | bPawnsBehind) ^ bTriplePawns;
//     double doublePawnValue = (opPhase * Eval::DoublePawnPenalty[MIDGAME]
//                             + egPhase * Eval::DoublePawnPenalty[ENDGAME]) / TOTALPHASE;
//     double doublePawnScore = (BB::bitCount(wDoublePawns) - BB::bitCount(bDoublePawns)) * doublePawnValue;
//     double triplePawnValue = (opPhase * Eval::TriplePawnPenalty[MIDGAME]
//                             + egPhase * Eval::TriplePawnPenalty[ENDGAME]) / TOTALPHASE;
//     double triplePawnScore = (BB::bitCount(wTriplePawns) - BB::bitCount(bTriplePawns)) * triplePawnValue;

//     // Isolated pawns
//     U64 wPawnsFill = BB::fillFiles(wPawns),
//         bPawnsFill = BB::fillFiles(bPawns);
//     U64 wIsolatedPawns = (wPawns & ~BB::shiftWest(wPawnsFill))
//                        & (wPawns & ~BB::shiftEast(wPawnsFill));
//     U64 bIsolatedPawns = (bPawns & ~BB::shiftWest(bPawnsFill))
//                        & (bPawns & ~BB::shiftEast(bPawnsFill));
//     double isoPawnValue = (opPhase * Eval::IsoPawnPenalty[MIDGAME]
//                         + egPhase * Eval::IsoPawnPenalty[ENDGAME]) / TOTALPHASE;
//     double isoPawnScore = (BB::bitCount(wIsolatedPawns) - BB::bitCount(bIsolatedPawns)) * isoPawnValue;
//     double pawnScore = passedPawnScore + doublePawnScore + triplePawnScore + isoPawnScore;

//     if (debug) {
// 		std::cout << " Pawns      |      -      |      -      | "
//                   << std::setw(6) << pawnScore << std::endl;
// 	}

//     // Give bonuses to queens/rooks on open/half open files
//     U64 openFiles = ~BB::fillFiles(allPawns),
//         wHalfOpenFiles = ~BB::fillFiles(wPawns) ^ openFiles,
//         bHalfOpenFiles = ~BB::fillFiles(bPawns) ^ openFiles,
//         wSliders = board.straightSliders(WHITE),
//         bSliders = board.straightSliders(BLACK);
//     double openFileValue = (opPhase * Eval::OpenFileBonus[MIDGAME]
//                           + egPhase * Eval::OpenFileBonus[ENDGAME]) / TOTALPHASE;
//     double openFileScore = (BB::bitCount(wSliders & openFiles) - BB::bitCount(bSliders & openFiles)) * openFileValue;
//     double halfOpenFileValue = (opPhase * Eval::HalfOpenFileBonus[MIDGAME]
//                               + egPhase * Eval::HalfOpenFileBonus[ENDGAME]) / TOTALPHASE;
//     double halfOpenFileScore = (BB::bitCount(wSliders & wHalfOpenFiles) - BB::bitCount(bSliders & bHalfOpenFiles)) * halfOpenFileValue;

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
//         if (Types::getRank(sq) == RANK1)
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
//         if (Types::getRank(sq) == RANK8)
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
//     if ((pieces & BB::WHITESQUARES) && (pieces & BB::BLACKSQUARES))
//         wBishopScore += (opPhase * Eval::BishopPairBonus[MIDGAME]
//                       + (egPhase * Eval::BishopPairBonus[ENDGAME])) / TOTALPHASE;
//     while (pieces)
//     {
//         Square sq = BB::advanced<WHITE>(pieces);
//         pieces &= BB::clear(sq);
//         wBishopScore += Eval::BISHOP_TROPISM[BB::DISTANCE[bking][sq]];
//         if (Types::getRank(sq) == RANK1)
//             wBishopScore += Eval::BACK_RANK_MINOR_PENALTY * openingModifier;
//         U64 validOutposts = BB::movesByPiece<BISHOP>(sq, occ) | BB::set(sq);
//         if (validOutposts & allOutposts)
//             wBishopScore += Eval::MINOR_OUTPOST_BONUS;
//     }
//     double bBishopScore = 0;
//     pieces = board.getPieces<BISHOP>(BLACK);
//     if ((pieces & BB::WHITESQUARES) && (pieces & BB::BLACKSQUARES))
//         bBishopScore += (opPhase * Eval::BishopPairBonus[MIDGAME]
//                       + (egPhase * Eval::BishopPairBonus[ENDGAME])) / TOTALPHASE;
//     while (pieces)
//     {
//         Square sq = BB::advanced<BLACK>(pieces);
//         pieces &= BB::clear(sq);
//         bBishopScore += Eval::BISHOP_TROPISM[BB::DISTANCE[wking][sq]];
//         if (Types::getRank(sq) == RANK8)
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
//         if (Types::getRank(sq) >= RANK7)
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
//         if (Types::getRank(sq) <= RANK2)
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
//     double rawKingScore = Eval::STRONG_KING_SHIELD_BONUS * (BB::bitCount(wShieldStrong) - BB::bitCount(bShieldStrong))
//                         + Eval::WEAK_KING_SHIELD_BONUS * (BB::bitCount(wShieldWeak) - BB::bitCount(bShieldWeak));
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
//                   << std::setw(6) << color * Eval::TEMPO_BONUS << std::endl
//                   << " Total      |      -      |      -      | "
//                   << std::setw(6) << score + Eval::TEMPO_BONUS << std::endl;
// 	}

//     return score + Eval::TEMPO_BONUS;
// }
