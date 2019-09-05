#include "board.hpp"

void Board::make(Move mv)
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
    PieceType fromPieceType = Types::getPieceType(getPiece(from)),
              toPieceType   = Types::getPieceType(getPiece(to));
    MoveType movetype = mv.type();

    // Make corrections if en passant move
    if (movetype == ENPASSANT) {
        toPieceType = PAWN;
        capturedPieceSq = Types::move<PawnMove::PUSH, false>(to, stm);
        squares[capturedPieceSq] = EMPTY;
    }

    // Store the captured piece type for undoing move
    state[ply].captured = toPieceType;

    Color enemy = ~stm;
    if (toPieceType)
    {
        // Reset half move clock for capture
        state[ply].hmClock = 0;

        // Remove captured piece from the board representation
        removePiece<true>(capturedPieceSq, enemy, toPieceType);

        // Disable castle rights if captured piece is rook
        if (canCastle(enemy) && toPieceType == ROOK)
        {
            if (to == MoveGen::RookOriginOO[enemy] && canCastleOO(enemy))
                disableCastleOO(enemy);
            else if (to == MoveGen::RookOriginOOO[enemy] && canCastleOOO(enemy))
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
        movePiece<true>(from, to, stm, fromPieceType);

    // Handle en passants, promotions, and update castling rights
    switch (fromPieceType) {

        case PAWN:
        {
            // Reset half move clock
            state[ply].hmClock = 0;

            if ((from - to) == 16 || (to - from) == 16)
            {
                // Set en passant square
                Square epsq = Types::move<PawnMove::PUSH, false>(to, stm);
                setEnPassant(epsq);
            }
            else if (movetype == PROMOTION)
            {
                // Promote the piece by replacing pawn with promotion piece
                removePiece<true>(to, stm, PAWN);
                addPiece<true>(to, stm, mv.promPiece());
            }
            break;
        }

        case KING:
        {
            // Update king square
            kingSq[stm] = to;

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
                if (from == MoveGen::RookOriginOO[stm] && canCastleOO(stm))
                    disableCastleOO(stm);
                else if (from == MoveGen::RookOriginOOO[stm] && canCastleOOO(stm))
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

void Board::unmake()
{
    // Get basic move information
    Color enemy = stm;
    Move mv = state[ply].move;
    Square from = mv.from(),
           to = mv.to();
    PieceType toPieceType = state[ply].captured,
              fromPieceType = Types::getPieceType(getPiece(to));
    MoveType movetype = mv.type();

    // Revert to the previous board state
    --ply;
    --fullMoveCounter;
    stm = ~stm;
    state.pop_back();

    // Make corrections if promotion move
    if (movetype == PROMOTION)
    {
        removePiece<false>(to, stm, fromPieceType);
        addPiece<false>(to, stm, PAWN);
        fromPieceType = PAWN;
    }

    // Undo the move
    if (movetype == CASTLE)
        makeCastle<false>(from, to, stm);
    else
    {
        movePiece<false>(to, from, stm, fromPieceType);

        // Add captured piece
        if (toPieceType)
        {
            Square capturedPieceSq = to;
            if (movetype == ENPASSANT)
                capturedPieceSq = Types::move<PawnMove::PUSH, false>(to, stm);

            addPiece<false>(capturedPieceSq, enemy, toPieceType);
        }
    }

    if (fromPieceType == KING)
        kingSq[stm] = from;
}

void Board::makeNull()
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

void Board::unmmakeNull()
{
    --ply;
    --fullMoveCounter;
    stm = ~stm;
    state.pop_back();
}

template<bool forward>
void Board::makeCastle(Square from, Square to, Color c)
{
    if (forward)
        movePiece<forward>(from, to, c, KING);
    else
        movePiece<forward>(to, from, c, KING);

    if (to > from) {
        to = Square(from + 1);
        from = MoveGen::RookOriginOO[c];
    }
    else {
        to = Square(from - 1);
        from = MoveGen::RookOriginOOO[c];
    }

    if (forward)
        movePiece<forward>(from, to, c, ROOK);
    else
        movePiece<forward>(to, from, c, ROOK);
}
