#include <iomanip>
#include "chess.hpp"

// Create a board using a FEN string
// See: https://www.chessprogramming.org/Forsyth-Edwards_Notation
Chess::Chess(const std::string& fen)
    : state{{ State() }}
    , board{ Board(fen) }
    , ply{0}
    , fullMoveCounter{0}
{
    loadFEN(fen);
    updateState();
}

void Chess::make(Move mv)
{
    // First check if move gives check, will save work later
    bool checkingMove = isCheckingMove(mv);
    // Get the en passant square before advancing the state
    Square epsq = getEnPassant();

    // Create new board state and push it onto board state stack
    state.push_back(State(state[ply], mv));
    ++ply;
    ++fullMoveCounter;

    // Get basic information about the move
    Square from = mv.from(),
           to = mv.to(),
           capturedPieceSq = to;
    PieceRole fromPieceRole = Types::getPieceRole(board.getPiece(from)),
              toPieceRole   = Types::getPieceRole(board.getPiece(to));
    MoveType movetype = mv.type();

    // Make corrections if en passant move
    if (movetype == ENPASSANT) {
        toPieceRole = PAWN;
        capturedPieceSq = Types::pawnMove<PawnMove::PUSH, false>(to, stm);
        board.squares[capturedPieceSq] = NO_PIECE;
    }

    // Store the captured piece role for undoing move
    state[ply].captured = toPieceRole;

    Color enemy = ~stm;
    if (toPieceRole)
    {
        // Reset half move clock for capture
        state[ply].hmClock = 0;

        // Remove captured piece from the board representation
        board.removePiece<true>(capturedPieceSq, enemy, toPieceRole);

        // Disable castle rights if captured piece is rook
        if (canCastle(enemy) && toPieceRole == ROOK)
        {
            if (to == G::RookOriginOO[enemy] && canCastleOO(enemy))
                disableCastleOO(enemy);
            else if (to == G::RookOriginOOO[enemy] && canCastleOOO(enemy))
                disableCastleOOO(enemy);
        }
    }

    // Remove ep square from zobrist key if any
    if (epsq != INVALID)
        state[ply].zkey ^= Zobrist::ep[Types::getFile(epsq)];

    // Move the piece
    if (movetype == CASTLE)
        makeCastle<true>(from, to, stm);
    else
        board.movePiece<true>(from, to, stm, fromPieceRole);

    // Handle en passants, promotions, and update castling rights
    switch (fromPieceRole) {

        case PAWN:
        {
            // Reset half move clock
            state[ply].hmClock = 0;

            if ((from - to) == 16 || (to - from) == 16)
            {
                // Set en passant square
                Square epsq = Types::pawnMove<PawnMove::PUSH, false>(to, stm);
                setEnPassant(epsq);
            }
            else if (movetype == PROMOTION)
            {
                // Promote the piece by replacing pawn with promotion piece
                board.removePiece<true>(to, stm, PAWN);
                board.addPiece<true>(to, stm, mv.promPiece());
            }
            break;
        }

        case KING:
        {
            // Update king square
            board.kingSq[stm] = to;

            // Disable castling rights
            if (canCastle(stm))
                disableCastle(stm);
            break;
        }

        case ROOK:
        {
            // Disable castling rights
            if (canCastle(stm))
            {
                if (from == G::RookOriginOO[stm] && canCastleOO(stm))
                    disableCastleOO(stm);
                else if (from == G::RookOriginOOO[stm] && canCastleOOO(stm))
                    disableCastleOOO(stm);
            }
            break;
        }

        default:
            break;
    }

    stm = enemy;
    state[ply].zkey ^= Zobrist::stm;

    updateState(checkingMove);
}

void Chess::unmake()
{
    // Get basic move information
    Color enemy = stm;
    Move mv = state[ply].move;
    Square from = mv.from(),
           to = mv.to();
    PieceRole toPieceRole = state[ply].captured,
              fromPieceRole = Types::getPieceRole(board.getPiece(to));
    MoveType movetype = mv.type();

    // Revert to the previous board state
    --ply;
    --fullMoveCounter;
    stm = ~stm;
    state.pop_back();

    // Make corrections if promotion move
    if (movetype == PROMOTION)
    {
        board.removePiece<false>(to, stm, fromPieceRole);
        board.addPiece<false>(to, stm, PAWN);
        fromPieceRole = PAWN;
    }

    // Undo the move
    if (movetype == CASTLE)
        makeCastle<false>(from, to, stm);
    else
    {
        board.movePiece<false>(to, from, stm, fromPieceRole);

        // Add captured piece
        if (toPieceRole)
        {
            Square capturedPieceSq = to;
            if (movetype == ENPASSANT)
                capturedPieceSq = Types::pawnMove<PawnMove::PUSH, false>(to, stm);

            board.addPiece<false>(capturedPieceSq, enemy, toPieceRole);
        }
    }

    if (fromPieceRole == KING)
        board.kingSq[stm] = from;
}

void Chess::makeNull()
{
    Square epsq = getEnPassant();

    state.push_back(State(state[ply], Move()));
    stm = ~stm;
    ++fullMoveCounter;
    ++ply;

    state[ply].zkey ^= Zobrist::stm;
    if (epsq != INVALID)
        state[ply].zkey ^= Zobrist::ep[Types::getFile(epsq)];

    updateState();
}

void Chess::unmmakeNull()
{
    --ply;
    --fullMoveCounter;
    stm = ~stm;
    state.pop_back();
}

template<bool forward>
void Chess::makeCastle(Square from, Square to, Color c)
{
    if (forward)
        board.movePiece<forward>(from, to, c, KING);
    else
        board.movePiece<forward>(to, from, c, KING);

    if (to > from) {
        to = Square(from + 1);
        from = G::RookOriginOO[c];
    }
    else {
        to = Square(from - 1);
        from = G::RookOriginOOO[c];
    }

    if (forward)
        board.movePiece<forward>(from, to, c, ROOK);
    else
        board.movePiece<forward>(to, from, c, ROOK);
}

template<bool debug>
int Chess::eval() const
{
    double totalphase = static_cast<double>(TOTALPHASE);

    // Taper the eval between opening and endgame values
    int opPhase = calculatePhase();
    int egPhase = (TOTALPHASE - opPhase);
    double openingModifier = opPhase / totalphase;

    // Determine position score
    double opening = board.openingScore * opPhase;
    double endgame = board.endgameScore * egPhase;
    double positionScore = (opening + endgame) / totalphase;

    if (debug)
	{
		std::cout << " Term       |   Opening   |   Endgame   |   Total   " << std::endl
			      << "------------+-------------+-------------+-----------" << std::endl
                  << " Material   |      -      |      -      | "
                  << std::setw(6) << board.materialScore << std::endl
                  << " Piece Sq   | " 
                  << std::setw(6) << opening / totalphase << std::setw(8) << " | "
                  << std::setw(6) << endgame / totalphase << std::setw(8) << " | "
                  << std::setw(6) << positionScore << std::endl;
	}

    // Evaluate pawn structure
    U64 wPawns = board.getPieces<PAWN>(WHITE),
        bPawns = board.getPieces<PAWN>(BLACK),
        allPawns = wPawns | bPawns;

    // Passed pawns
    U64 wPassedPawns = wPawns & ~BB::getAllFrontSpan<BLACK>(bPawns),
        bPassedPawns = bPawns & ~BB::getAllFrontSpan<WHITE>(wPawns);
    double passedPawnValue = (opPhase * G::PassedPawnBonus[OPENING]
                            + egPhase * G::PassedPawnBonus[ENDGAME]) / TOTALPHASE;
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
    double doublePawnValue = (opPhase * G::DoublePawnPenalty[OPENING]
                            + egPhase * G::DoublePawnPenalty[ENDGAME]) / TOTALPHASE;
    double doublePawnScore = (BB::bitCount(wDoublePawns) - BB::bitCount(bDoublePawns)) * doublePawnValue;
    double triplePawnValue = (opPhase * G::TriplePawnPenalty[OPENING]
                            + egPhase * G::TriplePawnPenalty[ENDGAME]) / TOTALPHASE;
    double triplePawnScore = (BB::bitCount(wTriplePawns) - BB::bitCount(bTriplePawns)) * triplePawnValue;

    // Isolated pawns
    U64 wPawnsFill = BB::fillFiles(wPawns),
        bPawnsFill = BB::fillFiles(bPawns);
    U64 wIsolatedPawns = (wPawns & ~BB::shiftWest(wPawnsFill))
                       & (wPawns & ~BB::shiftEast(wPawnsFill));
    U64 bIsolatedPawns = (bPawns & ~BB::shiftWest(bPawnsFill))
                       & (bPawns & ~BB::shiftEast(bPawnsFill));
    double isoPawnValue = (opPhase * G::IsoPawnPenalty[OPENING]
                        + egPhase * G::IsoPawnPenalty[ENDGAME]) / TOTALPHASE;
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
        wSliders = board.straightSliders(WHITE),
        bSliders = board.straightSliders(BLACK);
    double openFileValue = (opPhase * G::OpenFileBonus[OPENING]
                          + egPhase * G::OpenFileBonus[ENDGAME]) / TOTALPHASE;
    double openFileScore = (BB::bitCount(wSliders & openFiles) - BB::bitCount(bSliders & openFiles)) * openFileValue;
    double halfOpenFileValue = (opPhase * G::HalfOpenFileBonus[OPENING]
                              + egPhase * G::HalfOpenFileBonus[ENDGAME]) / TOTALPHASE;
    double halfOpenFileScore = (BB::bitCount(wSliders & wHalfOpenFiles) - BB::bitCount(bSliders & bHalfOpenFiles)) * halfOpenFileValue;

    // As pawns are captured, penalize knights and give bonus to rooks
    int capturedPawns = 16 - BB::bitCount(allPawns);
    int knightPen = (board.count<KNIGHT>(WHITE) - board.count<KNIGHT>(BLACK))
                  * (capturedPawns * G::KNIGHT_PENALTY_PER_PAWN);
    int rookBonus = (board.count<ROOK>(WHITE) - board.count<ROOK>(BLACK))
                  * (capturedPawns * G::ROOK_BONUS_PER_PAWN);

    /**
     *  Evaluate individual pieces
     */
    Square wking = board.getKingSq(WHITE),
           bking = board.getKingSq(BLACK);
    U64 occ = board.occupancy(),
        pieces = 0,
        wHoles = ~BB::getFrontAttackSpan<WHITE>(wPawns) & G::WHITEHOLES,
        bHoles = ~BB::getFrontAttackSpan<BLACK>(bPawns) & G::BLACKHOLES,
        wOutposts = bHoles & BB::attacksByPawns<WHITE>(wPawns),
        bOutposts = wHoles & BB::attacksByPawns<BLACK>(bPawns),
        allOutposts = wOutposts | bOutposts;

    // Knights
    double wKnightScore = 0;
    pieces = board.getPieces<KNIGHT>(WHITE);
    while (pieces)
    {
        Square sq = BB::advanced<WHITE>(pieces);
        pieces &= BB::clear(sq);
        wKnightScore += G::KNIGHT_TROPISM[G::DISTANCE[bking][sq]];
        if (Types::getRank(sq) == RANK1)
            wKnightScore += G::BACK_RANK_MINOR_PENALTY * openingModifier;
        U64 validOutposts = BB::movesByPiece<KNIGHT>(sq, occ) | BB::set(sq);
        if (validOutposts & allOutposts)
            wKnightScore += G::MINOR_OUTPOST_BONUS;
    }
    double bKnightScore = 0;
    pieces = board.getPieces<KNIGHT>(BLACK);
    while (pieces)
    {
        Square sq = BB::advanced<BLACK>(pieces);
        pieces &= BB::clear(sq);
        bKnightScore += G::KNIGHT_TROPISM[G::DISTANCE[wking][sq]];
        if (Types::getRank(sq) == RANK8)
            bKnightScore += G::BACK_RANK_MINOR_PENALTY * openingModifier;
        U64 validOutposts = BB::movesByPiece<KNIGHT>(sq, occ) | BB::set(sq);
        if (validOutposts & allOutposts)
            bKnightScore += G::MINOR_OUTPOST_BONUS;
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
    pieces = board.getPieces<BISHOP>(WHITE);
    if ((pieces & G::WHITESQUARES) && (pieces & G::BLACKSQUARES))
        wBishopScore += (opPhase * G::BishopPairBonus[OPENING]
                      + (egPhase * G::BishopPairBonus[ENDGAME])) / TOTALPHASE;
    while (pieces)
    {
        Square sq = BB::advanced<WHITE>(pieces);
        pieces &= BB::clear(sq);
        wBishopScore += G::BISHOP_TROPISM[G::DISTANCE[bking][sq]];
        if (Types::getRank(sq) == RANK1)
            wBishopScore += G::BACK_RANK_MINOR_PENALTY * openingModifier;
        U64 validOutposts = BB::movesByPiece<BISHOP>(sq, occ) | BB::set(sq);
        if (validOutposts & allOutposts)
            wBishopScore += G::MINOR_OUTPOST_BONUS;
    }
    double bBishopScore = 0;
    pieces = board.getPieces<BISHOP>(BLACK);
    if ((pieces & G::WHITESQUARES) && (pieces & G::BLACKSQUARES))
        bBishopScore += (opPhase * G::BishopPairBonus[OPENING]
                      + (egPhase * G::BishopPairBonus[ENDGAME])) / TOTALPHASE;
    while (pieces)
    {
        Square sq = BB::advanced<BLACK>(pieces);
        pieces &= BB::clear(sq);
        bBishopScore += G::BISHOP_TROPISM[G::DISTANCE[wking][sq]];
        if (Types::getRank(sq) == RANK8)
            bBishopScore += G::BACK_RANK_MINOR_PENALTY * openingModifier;
        U64 validOutposts = BB::movesByPiece<BISHOP>(sq, occ) | BB::set(sq);
        if (validOutposts & allOutposts)
            bBishopScore += G::MINOR_OUTPOST_BONUS;
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
    pieces = board.getPieces<ROOK>(WHITE);
    while (pieces)
    {
        Square sq = BB::advanced<WHITE>(pieces);
        pieces &= BB::clear(sq);
        wRookScore += G::ROOK_TROPISM[G::DISTANCE[bking][sq]];
        wRookScore += G::ROOK_TROPISM[G::DISTANCE[wking][sq]] * openingModifier;
        if (Types::getRank(sq) >= RANK7)
            wRookScore += G::ROOK_ON_SEVENTH_BONUS * openingModifier;
        if (BB::movesByPiece<ROOK>(sq, occ) & pieces)
            wRookScore += G::CONNECTED_ROOK_BONUS;
    }
    double bRookScore = 0;
    pieces = board.getPieces<ROOK>(BLACK);
    while (pieces)
    {
        Square sq = BB::advanced<BLACK>(pieces);
        pieces &= BB::clear(sq);
        bRookScore += G::ROOK_TROPISM[G::DISTANCE[wking][sq]];
        bRookScore += G::ROOK_TROPISM[G::DISTANCE[bking][sq]] * openingModifier;
        if (Types::getRank(sq) <= RANK2)
            bRookScore += G::ROOK_ON_SEVENTH_BONUS * openingModifier;
        if (BB::movesByPiece<ROOK>(sq, occ) & pieces)
            bRookScore += G::CONNECTED_ROOK_BONUS;
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
    pieces = board.getPieces<QUEEN>(WHITE);
    while (pieces)
    {
        Square sq = BB::advanced<WHITE>(pieces);
        pieces &= BB::clear(sq);
        wQueenScore += G::QUEEN_TROPISM[G::DISTANCE[bking][sq]];
        wQueenScore += G::QUEEN_TROPISM[G::DISTANCE[wking][sq]] * openingModifier;
    }
    double bQueenScore = 0;
    pieces = board.getPieces<QUEEN>(BLACK);
    while (pieces)
    {
        Square sq = BB::advanced<BLACK>(pieces);
        pieces &= BB::clear(sq);
        bQueenScore += G::QUEEN_TROPISM[G::DISTANCE[wking][sq]];
        bQueenScore += G::QUEEN_TROPISM[G::DISTANCE[bking][sq]] * openingModifier;
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
    U64 wShield = BB::kingShield<WHITE>(wking),
        bShield = BB::kingShield<BLACK>(bking),
        wShieldStrong = wShield & wPawns,
        bShieldStrong = bShield & bPawns,
        wShieldWeak = BB::shiftNorth(wShield) & wPawns,
        bShieldWeak = BB::shiftSouth(bShield) & bPawns;
    double rawKingScore = G::STRONG_KING_SHIELD_BONUS * (BB::bitCount(wShieldStrong) - BB::bitCount(bShieldStrong))
                        + G::WEAK_KING_SHIELD_BONUS * (BB::bitCount(wShieldWeak) - BB::bitCount(bShieldWeak));
    double kingScore = rawKingScore * std::min(16, (int)fullMoveCounter) / 16 * openingModifier;
    if (debug)
	{
		std::cout << " King Safety| "
                  << std::setw(6) << kingScore
                  << "      |      -      | "
                  << std::setw(6) << kingScore << std::endl;
	}

    // double mobilityScore = board.calculateMobilityScore(opPhase, egPhase) / TOTALPHASE;
    double mobilityScore = 0;
    int color = 2*stm - 1; // +1 when stm=WHITE, -1 when wtm=BLACK
    double score = color * (board.materialScore + positionScore + pawnScore +
                         pieceScore + kingScore + mobilityScore);

    if (debug)
	{
		std::cout << " Mobility   |      -      |      -      | "
                  << std::setw(6) << mobilityScore << std::endl
                  << "------------+-------------+-------------+----------" << std::endl
                  << " Sub Total  |      -      |      -      | "
                  << std::setw(6) << score << std::endl
                  << " Tempo      |      -      |      -      | "
                  << std::setw(6) << color * G::TEMPO_BONUS << std::endl
                  << " Total      |      -      |      -      | "
                  << std::setw(6) << score + G::TEMPO_BONUS << std::endl;
	}

    return score + G::TEMPO_BONUS;
}

// Instantiate eval functions
template int Chess::eval<true>() const;
template int Chess::eval<false>() const;

// Determine if a move is legal for the current board
bool Chess::isLegalMove(Move mv) const
{
    Square from = mv.from(),
           to = mv.to(),
           king = board.getKingSq(stm);

    if (from == king)
    {
        if (mv.type() == CASTLE)
            return true;
        else  // Check if destination sq is attacked by enemy
            return !board.attacksTo(to, ~stm, board.occupancy() ^ BB::set(from) ^ BB::set(to));
    }
    else if (mv.type() == ENPASSANT)
    {
        Square enemyPawn = Types::pawnMove<PawnMove::PUSH, false>(to, stm);
        U64 occ = (board.occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) | BB::set(to);

        // Check if captured pawn was blocking check
        return !(BB::movesByPiece<BISHOP>(king, occ) & board.diagonalSliders(~stm))
            && !(BB::movesByPiece<ROOK  >(king, occ) & board.straightSliders(~stm));
    }
    else
    {
        // Check if moved piece was pinned
        return !state.at(ply).pinnedPieces
            || !(state.at(ply).pinnedPieces & BB::set(from))
            || BB::bitsInline(from, to) & BB::set(king);
    }
}

// Determine if a move gives check for the current board
bool Chess::isCheckingMove(Move mv) const
{
    Square from = mv.from(),
           to = mv.to();
    PieceRole role = Types::getPieceRole(board.getPiece(from));

    // Check if destination+piece role directly attacks the king
    if (state[ply].checkingSquares[role] & BB::set(to)) {
        return true;
    }

    // Check if moved piece was blocking enemy king from attack
    Square king = board.getKingSq(~stm);
    if (state[ply].discoveredCheckers
        && (state[ply].discoveredCheckers & BB::set(from))
        && !(BB::bitsInline(from, to) & BB::set(king)))
    {
        return true;
    }

    switch (mv.type())
    {
        case NORMAL:
            return false;

        case PROMOTION:
        {
            // Check if a promotion attacks the enemy king
            U64 occ = board.occupancy() ^ BB::set(from);
            return BB::movesByPiece(to, mv.promPiece(), occ) & BB::set(king);
        }

        case ENPASSANT:
        {
            // Check if captured pawn was blocking enemy king from attack
            Square enemyPawn = Types::pawnMove<PawnMove::PUSH, false>(to, stm);
            U64 occ = (board.occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) | BB::set(to);

            return BB::movesByPiece<BISHOP>(king, occ) & board.diagonalSliders(stm)
                || BB::movesByPiece<ROOK>(king, occ)   & board.straightSliders(stm);
        }

        case CASTLE:
        {
            // Check if rook's destination after castling attacks enemy king
            Square rookFrom, rookTo;

            if (to > from) {
                rookTo = Square(from + 1);
                rookFrom = G::RookOriginOO[stm];
            }
            else {
                rookTo = Square(from - 1);
                rookFrom = G::RookOriginOOO[stm];
            }

            U64 occ = (board.occupancy() ^ BB::set(from) ^ BB::set(rookFrom)) | BB::set(to) | BB::set(rookTo);

            return BB::movesByPiece<ROOK>(rookTo, occ) & BB::set(king);
        }
        default:
            return false;
    }

    return false;
}

U64 Chess::calculateKey() const
{
    U64 zkey = 0x0;

    for (auto sq = A1; sq != INVALID; sq++)
    {
        auto piece = board.getPiece(sq);

        if (piece != NO_PIECE)
        {
            auto c = Types::getPieceColor(piece);
            auto pt = Types::getPieceRole(piece);
            zkey ^= Zobrist::psq[c][pt-1][sq];
        }
    }

    if (stm == BLACK)
        zkey ^= Zobrist::stm;

    auto sq = getEnPassant();
    if (sq != INVALID)
        zkey ^= Zobrist::ep[Types::getFile(sq)];

    if (canCastleOO(WHITE))
        zkey ^= Zobrist::castle[WHITE][KINGSIDE];

    if (canCastleOOO(WHITE))
        zkey ^= Zobrist::castle[WHITE][QUEENSIDE];

    if (canCastleOO(BLACK))
        zkey ^= Zobrist::castle[BLACK][KINGSIDE];

    if (canCastleOOO(BLACK))
        zkey ^= Zobrist::castle[BLACK][QUEENSIDE];

    return zkey;
}

std::string Chess::toFEN() const {
    std::ostringstream oss;

    for (Rank rank = RANK8; rank >= RANK1; rank--)
    {
        int emptyCount = 0;
        for (File file = FILE1; file <= FILE8; file++) {

            Piece p = board.getPiece(Types::getSquare(file, rank));
            if (p != NO_PIECE) {

                if (emptyCount > 0) {
                    oss << emptyCount;
                    emptyCount = 0;
                }
                oss << p;
            }
            else
                ++emptyCount;
        }

        if (emptyCount > 0) {
            oss << emptyCount;
            emptyCount = 0;
        }
        if (rank != RANK1)
            oss << '/';
    }

    if (stm)
        oss << " w ";
    else
        oss << " b ";

    if (canCastle(WHITE) || canCastle(BLACK))
    {
        if (canCastleOO(WHITE))
            oss << "K";
        if (canCastleOOO(WHITE))
            oss << "Q";
        if (canCastleOO(BLACK))
            oss << "k";
        if (canCastleOOO(BLACK))
            oss << "q";
    } else {
        oss << "-";
    }
    
    Square enpassant = getEnPassant();
    if (enpassant != INVALID) {
        oss << " " << enpassant << " ";
    } else {
        oss << " - ";
    }

    oss << +state.at(ply).hmClock << " " << (fullMoveCounter + 1) / 2;

    return oss.str();
}

void Chess::loadFEN(const std::string& fen) {
    std::vector<std::string> tokens = G::split(fen, ' ');

    if (tokens.size() > 3) {
        if (tokens.at(1)[0] == 'w') {
            stm = WHITE;
        } else {
            stm = BLACK;
            state[ply].zkey ^= Zobrist::stm;
        }

        state.at(ply).castle = NO_CASTLE;
        std::string castle = tokens.at(2);
        if (castle.find('-') == std::string::npos)
        {
            if (castle.find('K') != std::string::npos)
            {
                state.at(ply).castle |= WHITE_OO;
                state.at(ply).zkey ^= Zobrist::castle[WHITE][KINGSIDE];
            }

            if (castle.find('Q') != std::string::npos)
            {
                state.at(ply).castle |= WHITE_OOO;
                state.at(ply).zkey ^= Zobrist::castle[WHITE][QUEENSIDE];
            }

            if (castle.find('k') != std::string::npos)
            {
                state.at(ply).castle |= BLACK_OO;
                state.at(ply).zkey ^= Zobrist::castle[BLACK][KINGSIDE];
            }

            if (castle.find('q') != std::string::npos)
            {
                state.at(ply).castle |= BLACK_OOO;
                state.at(ply).zkey ^= Zobrist::castle[BLACK][QUEENSIDE];
            }
        }

        std::string square = tokens.at(3);
        if (square.compare("-") != 0) {
            setEnPassant(Types::getSquareFromStr(square));
        }
    }

    if (tokens.size() > 4) {
        state.at(ply).hmClock = std::stoi(tokens.at(4));
    }

    if (tokens.size() > 5) {
        fullMoveCounter = 2 * std::stoul(tokens.at(5));
    }
}

std::string Chess::DebugString() const {
    std::ostringstream oss;
    oss << this;
    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const Chess& chess) {
    os << chess.board << std::endl;
    os << chess.toFEN() << std::endl;
    // os << chess.state[chess.ply].zkey << std::endl;

    return os;
}
