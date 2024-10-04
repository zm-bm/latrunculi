#include "chess.hpp"

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
        capturedPieceSq = Types::pawnMove<PawnMove::PUSH, false>(to, turn);
        board.squares[capturedPieceSq] = NO_PIECE;
    }

    // Store the captured piece role for undoing move
    state[ply].captured = toPieceRole;

    Color enemy = ~turn;
    if (toPieceRole)
    {
        // Reset half move clock for capture
        state[ply].hmClock = 0;

        // Remove captured piece from the board representation
        removePiece<true>(capturedPieceSq, enemy, toPieceRole);

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
        makeCastle<true>(from, to, turn);
    else
        movePiece<true>(from, to, turn, fromPieceRole);

    // Handle en passants, promotions, and update castling rights
    switch (fromPieceRole) {

        case PAWN:
        {
            // Reset half move clock
            state[ply].hmClock = 0;

            if ((from - to) == 16 || (to - from) == 16)
            {
                // Set en passant square
                Square epsq = Types::pawnMove<PawnMove::PUSH, false>(to, turn);
                setEnPassant(epsq);
            }
            else if (movetype == PROMOTION)
            {
                // Promote the piece by replacing pawn with promotion piece
                removePiece<true>(to, turn, PAWN);
                addPiece<true>(to, turn, mv.promPiece());
            }
            break;
        }

        case KING:
        {
            // Update king square
            board.kingSq[turn] = to;

            // Disable castling rights
            if (canCastle(turn))
                disableCastle(turn);
            break;
        }

        case ROOK:
        {
            // Disable castling rights
            if (canCastle(turn))
            {
                if (from == G::RookOriginOO[turn] && canCastleOO(turn))
                    disableCastleOO(turn);
                else if (from == G::RookOriginOOO[turn] && canCastleOOO(turn))
                    disableCastleOOO(turn);
            }
            break;
        }

        default:
            break;
    }

    turn = enemy;
    state[ply].zkey ^= Zobrist::stm;

    updateState(checkingMove);
}

void Chess::unmake()
{
    // Get basic move information
    Color enemy = turn;
    Move mv = state[ply].move;
    Square from = mv.from(),
           to = mv.to();
    PieceRole toPieceRole = state[ply].captured,
              fromPieceRole = Types::getPieceRole(board.getPiece(to));
    MoveType movetype = mv.type();

    // Revert to the previous board state
    --ply;
    --fullMoveCounter;
    turn = ~turn;
    state.pop_back();

    // Make corrections if promotion move
    if (movetype == PROMOTION)
    {
        removePiece<false>(to, turn, fromPieceRole);
        addPiece<false>(to, turn, PAWN);
        fromPieceRole = PAWN;
    }

    // Undo the move
    if (movetype == CASTLE)
        makeCastle<false>(from, to, turn);
    else
    {
        movePiece<false>(to, from, turn, fromPieceRole);

        // Add captured piece
        if (toPieceRole)
        {
            Square capturedPieceSq = to;
            if (movetype == ENPASSANT)
                capturedPieceSq = Types::pawnMove<PawnMove::PUSH, false>(to, turn);

            addPiece<false>(capturedPieceSq, enemy, toPieceRole);
        }
    }

    if (fromPieceRole == KING)
        board.kingSq[turn] = from;
}

void Chess::makeNull()
{
    Square epsq = getEnPassant();

    state.push_back(State(state[ply], Move()));
    turn = ~turn;
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
    turn = ~turn;
    state.pop_back();
}

template<bool forward>
void Chess::makeCastle(Square from, Square to, Color c)
{
    if (forward)
        movePiece<forward>(from, to, c, KING);
    else
        movePiece<forward>(to, from, c, KING);

    if (to > from) {
        to = Square(from + 1);
        from = G::RookOriginOO[c];
    }
    else {
        to = Square(from - 1);
        from = G::RookOriginOOO[c];
    }

    if (forward)
        movePiece<forward>(from, to, c, ROOK);
    else
        movePiece<forward>(to, from, c, ROOK);
}



// Determine if a move is legal for the current board
bool Chess::isLegalMove(Move mv) const
{
    Square from = mv.from(),
           to = mv.to(),
           king = board.getKingSq(turn);

    if (from == king)
    {
        if (mv.type() == CASTLE)
            return true;
        else  // Check if destination sq is attacked by enemy
            return !board.attacksTo(to, ~turn, board.occupancy() ^ BB::set(from) ^ BB::set(to));
    }
    else if (mv.type() == ENPASSANT)
    {
        Square enemyPawn = Types::pawnMove<PawnMove::PUSH, false>(to, turn);
        U64 occ = (board.occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) | BB::set(to);

        // Check if captured pawn was blocking check
        return !(BB::movesByPiece<BISHOP>(king, occ) & board.diagonalSliders(~turn))
            && !(BB::movesByPiece<ROOK  >(king, occ) & board.straightSliders(~turn));
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
    Square king = board.getKingSq(~turn);
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
            Square enemyPawn = Types::pawnMove<PawnMove::PUSH, false>(to, turn);
            U64 occ = (board.occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) | BB::set(to);

            return BB::movesByPiece<BISHOP>(king, occ) & board.diagonalSliders(turn)
                || BB::movesByPiece<ROOK>(king, occ)   & board.straightSliders(turn);
        }

        case CASTLE:
        {
            // Check if rook's destination after castling attacks enemy king
            Square rookFrom, rookTo;

            if (to > from) {
                rookTo = Square(from + 1);
                rookFrom = G::RookOriginOO[turn];
            }
            else {
                rookTo = Square(from - 1);
                rookFrom = G::RookOriginOOO[turn];
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
            auto p = Types::getPieceRole(piece);
            zkey ^= Zobrist::psq[c][p-1][sq];
        }
    }

    if (turn == BLACK)
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

    if (turn)
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

Chess::Chess(const std::string& fen)
    : state{{ State() }}
    , board{ Board(fen) }
    , ply{0}
    , fullMoveCounter{0}
{
    // Constructor using a FEN string
    // See: https://www.chessprogramming.org/Forsyth-Edwards_Notation
    std::vector<std::string> tokens = G::split(fen, ' ');

    if (tokens.size() > 3) {
        if (tokens.at(1)[0] == 'w') {
            turn = WHITE;
        } else {
            turn = BLACK;
        }

        state.at(ply).castle = NO_CASTLE;
        std::string castle = tokens.at(2);
        if (castle.find('-') == std::string::npos) {
            if (castle.find('K') != std::string::npos) {
                state.at(ply).castle |= WHITE_OO;
            }

            if (castle.find('Q') != std::string::npos) {
                state.at(ply).castle |= WHITE_OOO;
            }

            if (castle.find('k') != std::string::npos) {
                state.at(ply).castle |= BLACK_OO;
            }

            if (castle.find('q') != std::string::npos) {
                state.at(ply).castle |= BLACK_OOO;
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

    state.at(ply).zkey = calculateKey();
    updateState();
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
