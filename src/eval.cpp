#include <iomanip>
#include "eval.hpp"
#include "board.hpp"
#include "types.hpp"

const U64 WHITESQUARES = 0x55AA55AA55AA55AA;
const U64 BLACKSQUARES = 0xAA55AA55AA55AA55;
const U64 WHITEHOLES   = 0x0000003CFFFF0000;
const U64 BLACKHOLES   = 0x0000FFFF3C000000;

template<bool debug>
int Board::eval() const
{
    double totalphase = static_cast<double>(TOTALPHASE);

    // Taper the eval between opening and endgame values
    int opPhase = calculatePhase();
    int egPhase = (TOTALPHASE - opPhase);
    double openingModifier = opPhase / totalphase;

    // Determine position score
    double opening = openingScore * opPhase;
    double endgame = endgameScore * egPhase;
    double positionScore = (opening + endgame) / totalphase;

    if (debug)
	{
		std::cout << " Term       |   Opening   |   Endgame   |   Total   " << std::endl
			      << "------------+-------------+-------------+-----------" << std::endl
                  << " Material   |      -      |      -      | "
                  << std::setw(6) << materialScore << std::endl
                  << " Piece Sq   | " 
                  << std::setw(6) << opening / totalphase << std::setw(8) << " | "
                  << std::setw(6) << endgame / totalphase << std::setw(8) << " | "
                  << std::setw(6) << positionScore << std::endl;
	}

    // Evaluate pawn structure
    U64 wPawns = getPieces<PAWN>(WHITE),
        bPawns = getPieces<PAWN>(BLACK),
        allPawns = wPawns | bPawns;

    // Passed pawns
    U64 wPassedPawns = wPawns & ~BB::getAllFrontSpan<BLACK>(bPawns),
        bPassedPawns = bPawns & ~BB::getAllFrontSpan<WHITE>(wPawns);
    double passedPawnValue = (opPhase * Eval::PassedPawnBonus[OPENING]
                            + egPhase * Eval::PassedPawnBonus[ENDGAME]) / TOTALPHASE;
    double passedPawnScore = (BB::bitCount(wPassedPawns) - BB::bitCount(bPassedPawns)) * passedPawnValue;

    // Doubled+tripled pawns
    U64 wPawnsAhead  = wPawns & ~BB::spanFront<WHITE>(wPawns),
        wPawnsBehind = wPawns & ~BB::spanBack<WHITE>(wPawns),
        wTriplePawns = wPawnsAhead & wPawnsBehind,
        wDoublePawns = (wPawnsAhead | wPawnsBehind) ^ wTriplePawns,
        bPawnsAhead  = bPawns & ~BB::spanFront<BLACK>(bPawns),
        bPawnsBehind = bPawns & ~BB::spanBack<BLACK>(bPawns),
        bTriplePawns = bPawnsAhead & bPawnsBehind,
        bDoublePawns = (bPawnsAhead | bPawnsBehind) ^ bTriplePawns;
    double doublePawnValue = (opPhase * Eval::DoublePawnPenalty[OPENING]
                            + egPhase * Eval::DoublePawnPenalty[ENDGAME]) / TOTALPHASE;
    double doublePawnScore = (BB::bitCount(wDoublePawns) - BB::bitCount(bDoublePawns)) * doublePawnValue;
    double triplePawnValue = (opPhase * Eval::TriplePawnPenalty[OPENING]
                            + egPhase * Eval::TriplePawnPenalty[ENDGAME]) / TOTALPHASE;
    double triplePawnScore = (BB::bitCount(wTriplePawns) - BB::bitCount(bTriplePawns)) * triplePawnValue;

    // Isolated pawns
    U64 wPawnsFill = BB::fillFiles(wPawns),
        bPawnsFill = BB::fillFiles(bPawns);
    U64 wIsolatedPawns = (wPawns & ~BB::shiftWest(wPawnsFill))
                       & (wPawns & ~BB::shiftEast(wPawnsFill));
    U64 bIsolatedPawns = (bPawns & ~BB::shiftWest(bPawnsFill))
                       & (bPawns & ~BB::shiftEast(bPawnsFill));
    double isoPawnValue = (opPhase * Eval::IsoPawnPenalty[OPENING]
                        + egPhase * Eval::IsoPawnPenalty[ENDGAME]) / TOTALPHASE;
    double isoPawnScore = (BB::bitCount(wIsolatedPawns) - BB::bitCount(bIsolatedPawns)) * isoPawnValue;
    double pawnScore = passedPawnScore + doublePawnScore + triplePawnScore + isoPawnScore;

    if (debug) {
		std::cout << " Pawns      |      -      |      -      | "
                  << std::setw(6) << pawnScore << std::endl;
	}

    // Give bonuses to queens/rooks on open/half open files
    U64 openFiles = ~BB::fillFiles(allPawns),
        wHalfOpenFiles = ~BB::fillFiles(wPawns) ^ openFiles,
        bHalfOpenFiles = ~BB::fillFiles(bPawns) ^ openFiles,
        wSliders = straightSliders(WHITE),
        bSliders = straightSliders(BLACK);
    double openFileValue = (opPhase * Eval::OpenFileBonus[OPENING]
                          + egPhase * Eval::OpenFileBonus[ENDGAME]) / TOTALPHASE;
    double openFileScore = (BB::bitCount(wSliders & openFiles) - BB::bitCount(bSliders & openFiles)) * openFileValue;
    double halfOpenFileValue = (opPhase * Eval::HalfOpenFileBonus[OPENING]
                              + egPhase * Eval::HalfOpenFileBonus[ENDGAME]) / TOTALPHASE;
    double halfOpenFileScore = (BB::bitCount(wSliders & wHalfOpenFiles) - BB::bitCount(bSliders & bHalfOpenFiles)) * halfOpenFileValue;

    // As pawns are captured, penalize knights and give bonus to rooks
    int capturedPawns = 16 - BB::bitCount(allPawns);
    int knightPen = (count<KNIGHT>(WHITE) - count<KNIGHT>(BLACK))
                  * (capturedPawns * Eval::KNIGHT_PENALTY_PER_PAWN);
    int rookBonus = (count<ROOK>(WHITE) - count<ROOK>(BLACK))
                  * (capturedPawns * Eval::ROOK_BONUS_PER_PAWN);

    /**
     *  Evaluate individual pieces
     */
    Square wking = getKingSq(WHITE),
           bking = getKingSq(BLACK);
    U64 occ = occupancy(),
        pieces = 0,
        wHoles = ~BB::getFrontAttackSpan<WHITE>(wPawns) & WHITEHOLES,
        bHoles = ~BB::getFrontAttackSpan<BLACK>(bPawns) & BLACKHOLES,
        wOutposts = bHoles & MoveGen::attacksByPawns<WHITE>(wPawns),
        bOutposts = wHoles & MoveGen::attacksByPawns<BLACK>(bPawns),
        allOutposts = wOutposts | bOutposts;

    // Knights
    double wKnightScore = 0;
    pieces = getPieces<KNIGHT>(WHITE);
    while (pieces)
    {
        Square sq = BB::advanced<WHITE>(pieces);
        pieces &= BB::clear(sq);
        wKnightScore += Eval::KNIGHT_TROPISM[G::DISTANCE[bking][sq]];
        if (Types::getRank(sq) == RANK1)
            wKnightScore += Eval::BACK_RANK_MINOR_PENALTY * openingModifier;
        U64 validOutposts = MoveGen::movesByPiece<KNIGHT>(sq, occ) | BB::set(sq);
        if (validOutposts & allOutposts)
            wKnightScore += Eval::MINOR_OUTPOST_BONUS;
    }
    double bKnightScore = 0;
    pieces = getPieces<KNIGHT>(BLACK);
    while (pieces)
    {
        Square sq = BB::advanced<BLACK>(pieces);
        pieces &= BB::clear(sq);
        bKnightScore += Eval::KNIGHT_TROPISM[G::DISTANCE[wking][sq]];
        if (Types::getRank(sq) == RANK8)
            bKnightScore += Eval::BACK_RANK_MINOR_PENALTY * openingModifier;
        U64 validOutposts = MoveGen::movesByPiece<KNIGHT>(sq, occ) | BB::set(sq);
        if (validOutposts & allOutposts)
            bKnightScore += Eval::MINOR_OUTPOST_BONUS;
    }
    if (debug)
	{
		std::cout << " W Knights  |      -      |      -      | "
                  << std::setw(6) << wKnightScore << std::endl
                  << " B Knights  |      -      |      -      | "
                  << std::setw(6) << -bKnightScore << std::endl;
	}

    // Bishops
    double wBishopScore = 0;
    pieces = getPieces<BISHOP>(WHITE);
    if ((pieces & WHITESQUARES) && (pieces & BLACKSQUARES))
        wBishopScore += (opPhase * Eval::BishopPairBonus[OPENING]
                      + (egPhase * Eval::BishopPairBonus[ENDGAME])) / TOTALPHASE;
    while (pieces)
    {
        Square sq = BB::advanced<WHITE>(pieces);
        pieces &= BB::clear(sq);
        wBishopScore += Eval::BISHOP_TROPISM[G::DISTANCE[bking][sq]];
        if (Types::getRank(sq) == RANK1)
            wBishopScore += Eval::BACK_RANK_MINOR_PENALTY * openingModifier;
        U64 validOutposts = MoveGen::movesByPiece<BISHOP>(sq, occ) | BB::set(sq);
        if (validOutposts & allOutposts)
            wBishopScore += Eval::MINOR_OUTPOST_BONUS;
    }
    double bBishopScore = 0;
    pieces = getPieces<BISHOP>(BLACK);
    if ((pieces & WHITESQUARES) && (pieces & BLACKSQUARES))
        bBishopScore += (opPhase * Eval::BishopPairBonus[OPENING]
                      + (egPhase * Eval::BishopPairBonus[ENDGAME])) / TOTALPHASE;
    while (pieces)
    {
        Square sq = BB::advanced<BLACK>(pieces);
        pieces &= BB::clear(sq);
        bBishopScore += Eval::BISHOP_TROPISM[G::DISTANCE[wking][sq]];
        if (Types::getRank(sq) == RANK8)
            bBishopScore += Eval::BACK_RANK_MINOR_PENALTY * openingModifier;
        U64 validOutposts = MoveGen::movesByPiece<BISHOP>(sq, occ) | BB::set(sq);
        if (validOutposts & allOutposts)
            bBishopScore += Eval::MINOR_OUTPOST_BONUS;
    }
    if (debug)
	{
		std::cout << " W Bishops  |      -      |      -      | "
                  << std::setw(6) << wBishopScore << std::endl
                  << " B Bishops  |      -      |      -      | "
                  << std::setw(6) << -bBishopScore << std::endl;
	}

    // Rooks
    double wRookScore = 0;
    pieces = getPieces<ROOK>(WHITE);
    while (pieces)
    {
        Square sq = BB::advanced<WHITE>(pieces);
        pieces &= BB::clear(sq);
        wRookScore += Eval::ROOK_TROPISM[G::DISTANCE[bking][sq]];
        wRookScore += Eval::ROOK_TROPISM[G::DISTANCE[wking][sq]] * openingModifier;
        if (Types::getRank(sq) >= RANK7)
            wRookScore += Eval::ROOK_ON_SEVENTH_BONUS * openingModifier;
        if (MoveGen::movesByPiece<ROOK>(sq, occ) & pieces)
            wRookScore += Eval::CONNECTED_ROOK_BONUS;
    }
    double bRookScore = 0;
    pieces = getPieces<ROOK>(BLACK);
    while (pieces)
    {
        Square sq = BB::advanced<BLACK>(pieces);
        pieces &= BB::clear(sq);
        bRookScore += Eval::ROOK_TROPISM[G::DISTANCE[wking][sq]];
        bRookScore += Eval::ROOK_TROPISM[G::DISTANCE[bking][sq]] * openingModifier;
        if (Types::getRank(sq) <= RANK2)
            bRookScore += Eval::ROOK_ON_SEVENTH_BONUS * openingModifier;
        if (MoveGen::movesByPiece<ROOK>(sq, occ) & pieces)
            bRookScore += Eval::CONNECTED_ROOK_BONUS;
    }
    if (debug)
	{
		std::cout << " W Rooks    |      -      |      -      | "
                  << std::setw(6) << wRookScore << std::endl
                  << " B Rooks    |      -      |      -      | "
                  << std::setw(6) << -bRookScore << std::endl;
	}

    // Queens
    double wQueenScore = 0;
    pieces = getPieces<QUEEN>(WHITE);
    while (pieces)
    {
        Square sq = BB::advanced<WHITE>(pieces);
        pieces &= BB::clear(sq);
        wQueenScore += Eval::QUEEN_TROPISM[G::DISTANCE[bking][sq]];
        wQueenScore += Eval::QUEEN_TROPISM[G::DISTANCE[wking][sq]] * openingModifier;
    }
    double bQueenScore = 0;
    pieces = getPieces<QUEEN>(BLACK);
    while (pieces)
    {
        Square sq = BB::advanced<BLACK>(pieces);
        pieces &= BB::clear(sq);
        bQueenScore += Eval::QUEEN_TROPISM[G::DISTANCE[wking][sq]];
        bQueenScore += Eval::QUEEN_TROPISM[G::DISTANCE[bking][sq]] * openingModifier;
    }
    if (debug)
	{
		std::cout << " W Queens   |      -      |      -      | "
                  << std::setw(6) << wQueenScore << std::endl
                  << " B Queens   |      -      |      -      | "
                  << std::setw(6) << -bQueenScore << std::endl;
	}

    double pieceScore = (wKnightScore - bKnightScore)
                      + (wBishopScore - bBishopScore)
                      + (wRookScore - bRookScore)
                      + (wQueenScore - bQueenScore)
                      + openFileScore + halfOpenFileScore + knightPen + rookBonus;
    if (debug)
	{
		std::cout << " Open files |      -      |      -      | "
                  << std::setw(6) << openFileScore + halfOpenFileScore << std::endl
                  << " Pawns adj. |      -      |      -      | "
                  << std::setw(6) << knightPen + rookBonus << std::endl;
	}

    // Kings
    U64 wShield = Eval::kingShield<WHITE>(wking),
        bShield = Eval::kingShield<BLACK>(bking),
        wShieldStrong = wShield & wPawns,
        bShieldStrong = bShield & bPawns,
        wShieldWeak = BB::shiftNorth(wShield) & wPawns,
        bShieldWeak = BB::shiftSouth(bShield) & bPawns;
    double rawKingScore = Eval::STRONG_KING_SHIELD_BONUS * (BB::bitCount(wShieldStrong) - BB::bitCount(bShieldStrong))
                        + Eval::WEAK_KING_SHIELD_BONUS * (BB::bitCount(wShieldWeak) - BB::bitCount(bShieldWeak));
    double kingScore = rawKingScore * std::min(16, (int)fullMoveCounter) / 16 * openingModifier;
    if (debug)
	{
		std::cout << " King Safety| "
                  << std::setw(6) << kingScore
                  << "      |      -      | "
                  << std::setw(6) << kingScore << std::endl;
	}

    double mobilityScore = calculateMobilityScore(opPhase, egPhase) / TOTALPHASE;
    int color = 2*stm - 1; // +1 when stm=WHITE, -1 when wtm=BLACK
    double score = color * (materialScore + positionScore + pawnScore +
                         pieceScore + kingScore + mobilityScore);

    if (debug)
	{
		std::cout << " Mobility   |      -      |      -      | "
                  << std::setw(6) << mobilityScore << std::endl
                  << "------------+-------------+-------------+----------" << std::endl
                  << " Sub Total  |      -      |      -      | "
                  << std::setw(6) << score << std::endl
                  << " Tempo      |      -      |      -      | "
                  << std::setw(6) << color * Eval::TEMPO_BONUS << std::endl
                  << " Total      |      -      |      -      | "
                  << std::setw(6) << score + Eval::TEMPO_BONUS << std::endl;
	}

    return score + Eval::TEMPO_BONUS;
}

// Instantiate eval functions
template int Board::eval<true>() const;
template int Board::eval<false>() const;

namespace Eval
{

    const int MobilityScaling[2][6] = {
        { 0,6,2,0,0,0 },
        { 2,3,1,1,1,1 }
    };

    const int PassedPawnBonus[2]    = {  30,  200 };
    const int DoublePawnPenalty[2]  = { -30, -100 };
    const int TriplePawnPenalty[2]  = { -45, -100 };
    const int IsoPawnPenalty[2]     = { -30,  -40 };
    const int OpenFileBonus[2]      = {  20,   10 };
    const int HalfOpenFileBonus[2]  = {  10,    0 };
    const int BishopPairBonus[2]    = {  20,   60 };

    const int TEMPO_BONUS                 = 25;
    const int KNIGHT_PENALTY_PER_PAWN     = -2;
    const int ROOK_BONUS_PER_PAWN         = 2;
    const int CONNECTED_ROOK_BONUS        = 15;
    const int ROOK_ON_SEVENTH_BONUS       = 20;
    const int BACK_RANK_MINOR_PENALTY     = -6;
    const int MINOR_OUTPOST_BONUS         = 10;
    const int STRONG_KING_SHIELD_BONUS    = 10;
    const int WEAK_KING_SHIELD_BONUS      = 5;

    const int PieceValues[6][2] = {
        { -PAWNSCORE,   PAWNSCORE },
        { -KNIGHTSCORE, KNIGHTSCORE },
        { -BISHOPSCORE, BISHOPSCORE },
        { -ROOKSCORE,   ROOKSCORE },
        { -QUEENSCORE,  QUEENSCORE },
        { -KINGSCORE,   KINGSCORE }
    };

    const int KNIGHT_TROPISM[8] = {
        0, 5, 4, 2, 0, 0, -1, -3
    };

    const int BISHOP_TROPISM[8] = {
        0, 5, 4, 3, 2, 1, 0, 0
    };

    const int ROOK_TROPISM[8] = {
        0, 6, 5, 3, 2, 1, 0, 0
    };

    const int QUEEN_TROPISM[8] = {
        0, 12, 10, 6, 4, 2, 0, -2
    };

    const int QUEEN_EARLY_DEV_PENALTY[4] = {
        0, -2, -8, -24
    };

    const int PieceSqValues[6][2][64] =
    {
        { // Pawn piece square values
            {
                0,  0,  0,  0,  0,  0,  0,  0,
                50, 50, 50, 50, 50, 50, 50, 50,
                10, 10, 20, 30, 30, 20, 10, 10,
                5,  5, 10, 25, 25, 10,  5,  5,
                0,  0,  0, 20, 20,  0,  0,  0,
                5, -5,-10,  0,  0,-10, -5,  5,
                5, 10, 10,-20,-20, 10, 10,  5,
                0,  0,  0,  0,  0,  0,  0,  0
            }, // Opening

            {
                 0,  0,  0,  0,  0,  0,  0,  0,
               115,125,125,125,125,125,125,125,
                85, 95, 95,105,105, 95, 95, 85,
                75, 85, 90,100,100, 90, 85, 65,
                65, 80, 80, 95, 95, 80, 80, 65,
                55, 75, 75, 75, 75, 75, 75, 55,
                50, 70, 70, 70, 70, 70, 70, 50,
                 0,  0,  0,  0,  0,  0,  0,  0
            } // End game
        },

        { // Knight piece square values 
            {
                -50,-40,-30,-30,-30,-30,-40,-50,
                -40,-20,  0,  0,  0,  0,-20,-40,
                -30,  0, 10, 15, 15, 10,  0,-30,
                -30,  5, 15, 20, 20, 15,  5,-30,
                -30,  0, 15, 20, 20, 15,  0,-30,
                -30,  5, 10, 15, 15, 10,  5,-30,
                -40,-20,  0,  5,  5,  0,-20,-40,
                -50,-40,-30,-30,-30,-30,-40,-50,
            }, // Opening

            {
                -50,-40,-30,-30,-30,-30,-40,-50,
                -40,-20,  0,  0,  0,  0,-20,-40,
                -30,  0, 10, 15, 15, 10,  0,-30,
                -30,  5, 15, 20, 20, 15,  5,-30,
                -30,  0, 15, 20, 20, 15,  0,-30,
                -30,  5, 10, 15, 15, 10,  5,-30,
                -40,-20,  0,  5,  5,  0,-20,-40,
                -50,-40,-30,-30,-30,-30,-40,-50,
            }  // End game
        },

        { // Bishop piece square values
            {
                -20,-10,-10,-10,-10,-10,-10,-20,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -10,  0,  5, 10, 10,  5,  0,-10,
                -10,  5,  5, 10, 10,  5,  5,-10,
                -10,  0, 10, 10, 10, 10,  0,-10,
                -10, 10, 10, 10, 10, 10, 10,-10,
                -10, 10,  0,  0,  0,  0, 10,-10,
                -20,-10,-10,-10,-10,-10,-10,-20
            }, // Opening

            {
                -20,-10,-10,-10,-10,-10,-10,-20,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -10,  0,  5, 10, 10,  5,  0,-10,
                -10,  5,  5, 10, 10,  5,  5,-10,
                -10,  0, 10, 10, 10, 10,  0,-10,
                -10, 10, 10, 10, 10, 10, 10,-10,
                -10,  5,  0,  0,  0,  0,  5,-10,
                -20,-10,-10,-10,-10,-10,-10,-20
            }  // End game
        },

        { // Rook piece square values
            {
                  0,  0,  0,  0,  0,  0,  0,  0,
                  5, 10, 10, 10, 10, 10, 10,  5,
                 -5,  0,  0,  0,  0,  0,  0, -5,
                 -5,  0,  0,  0,  0,  0,  0, -5,
                 -5,  0,  0,  0,  0,  0,  0, -5,
                 -5,  0,  0,  0,  0,  0,  0, -5,
                 -5,  0,  0,  0,  0,  0,  0, -5,
                  0,  0,  0,  5,  5,  0,  0,  0
            }, // Opening

            {
                  0,  0,  0,  0,  0,  0,  0,  0,
                  5, 10, 10, 10, 10, 10, 10,  5,
                 -5,  0,  0,  0,  0,  0,  0, -5,
                 -5,  0,  0,  0,  0,  0,  0, -5,
                 -5,  0,  0,  0,  0,  0,  0, -5,
                 -5,  0,  0,  0,  0,  0,  0, -5,
                 -5,  0,  0,  0,  0,  0,  0, -5,
                  0,  0,  0,  5,  5,  0,  0,  0
            }  // End game
        },

        { // Queen piece square values 
            {
                -20,-10,-10, -5, -5,-10,-10,-20,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -10,  0,  5,  5,  5,  5,  0,-10,
                 -5,  0,  5,  5,  5,  5,  0, -5,
                  0,  0,  5,  5,  5,  5,  0, -5,
                -10,  5,  5,  5,  5,  5,  0,-10,
                -10,  0,  5,  0,  0,  0,  0,-10,
                -20,-10,-10, -5, -5,-10,-10,-20
            }, // Opening

            {
                -20,-10,-10, -5, -5,-10,-10,-20,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -10,  0,  5,  5,  5,  5,  0,-10,
                 -5,  0,  5,  5,  5,  5,  0, -5,
                  0,  0,  5,  5,  5,  5,  0, -5,
                -10,  5,  5,  5,  5,  5,  0,-10,
                -10,  0,  5,  0,  0,  0,  0,-10,
                -20,-10,-10, -5, -5,-10,-10,-20
            }  // End game
        },

        { // King piece square values 
            {
                 30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -20,-30,-30,-40,-40,-30,-30,-20,
                -10,-20,-20,-20,-20,-20,-20,-10,
                 20, 20,  0,  0,  0,  0, 20, 20,
                 20, 30, 10,  0,  0, 10, 30, 20
            }, // Opening 

            {
                -50,-40,-30,-20,-20,-30,-40,-50,
                -30,-20,-10,  0,  0,-10,-20,-30,
                -30,-10, 20, 30, 30, 20,-10,-30,
                -30,-10, 30, 40, 40, 30,-10,-30,
                -30,-10, 30, 40, 40, 30,-10,-30,
                -30,-10, 20, 30, 30, 20,-10,-30,
                -30,-30,  0,  0,  0,  0,-30,-30,
                -50,-30,-30,-30,-30,-30,-30,-50
            }  // End game
        }
    };

    const Square ColorSq[2][64] = {
        {
            A1, B1, C1, D1, E1, F1, G1, H1,
            A2, B2, C2, D2, E2, F2, G2, H2,
            A3, B3, C3, D3, E3, F3, G3, H3,
            A4, B4, C4, D4, E4, F4, G4, H4,
            A5, B5, C5, D5, E5, F5, G5, H5,
            A6, B6, C6, D6, E6, F6, G6, H6,
            A7, B7, C7, D7, E7, F7, G7, H7,
            A8, B8, C8, D8, E8, F8, G8, H8,
        },

        {
            H8, G8, F8, E8, D8, C8, B8, A8,
            H7, G7, F7, E7, D7, C7, B7, A7,
            H6, G6, F6, E6, D6, C6, B6, A6,
            H5, G5, F5, E5, D5, C5, B5, A5,
            H4, G4, F4, E4, D4, C4, B4, A4,
            H3, G3, F3, E3, D3, C3, B3, A3,
            H2, G2, F2, E2, D2, C2, B2, A2,
            H1, G1, F1, E1, D1, C1, B1, A1,
        }
    };
}