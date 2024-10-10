#include "chess.hpp"

void Chess::make(Move mv) {
    // Get basic information about the move
    bool checkingMove = isCheckingMove(mv);
    Square from = mv.from();
    Square to = mv.to();
    Square capturedPieceSq = to;
    Square epsq = getEnPassant();
    PieceRole fromPieceRole = Types::getPieceRole(board.getPiece(from));
    PieceRole toPieceRole = Types::getPieceRole(board.getPiece(to));
    MoveType movetype = mv.type();
    Color enemy = ~turn;
    if (movetype == ENPASSANT) {
        toPieceRole = PAWN;
        capturedPieceSq = Types::pawnMove<PawnMove::PUSH, false>(to, turn);
        board.squares[capturedPieceSq] = NO_PIECE;
    }

    // Create new board state and push it onto board state stack
    state.push_back(State(state[ply], mv));
    ++ply;
    ++moveCounter;

    // Store the captured piece role for undoing move
    state[ply].captured = toPieceRole;
    if (toPieceRole) {
        updateCapturedPieces(capturedPieceSq, enemy, toPieceRole);
    }

    // Remove ep square from zobrist key if any
    if (epsq != INVALID) {
        state[ply].zkey ^= Zobrist::ep[Types::getFile(epsq)];
    }

    // Move the piece
    if (movetype == CASTLE) {
        makeCastle<true>(from, to, turn);
    } else {
        movePiece<true>(from, to, turn, fromPieceRole);
    }

    // Handle en passants, promotions, and update castling rights
    switch (fromPieceRole) {
        case PAWN: {
            handlePawnMoves(from, to, movetype, mv);
            break;
        }

        case KING: {
            board.kingSq[turn] = to;
            if (canCastle(turn)) {
                disableCastle(turn);
            }
            break;
        }

        case ROOK: {
            if (canCastle(turn)) {
                disableCastle(turn, from);
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

void Chess::unmake() {
    // Get basic move information
    Color enemy = turn;
    Move mv = state[ply].move;
    Square from = mv.from(), to = mv.to();
    PieceRole toPieceRole = state[ply].captured;
    PieceRole fromPieceRole = Types::getPieceRole(board.getPiece(to));
    MoveType movetype = mv.type();

    // Revert to the previous board state
    --ply;
    --moveCounter;
    turn = ~turn;
    state.pop_back();

    // Make corrections if promotion move
    if (movetype == PROMOTION) {
        removePiece<false>(to, turn, fromPieceRole);
        addPiece<false>(to, turn, PAWN);
        fromPieceRole = PAWN;
    }

    // Undo the move
    if (movetype == CASTLE) {
        makeCastle<false>(from, to, turn);
    } else {
        movePiece<false>(to, from, turn, fromPieceRole);
        if (toPieceRole) {
            Square capturedPieceSq = (movetype == ENPASSANT)
                ? Types::pawnMove<PawnMove::PUSH, false>(to, turn)
                : to;
            addPiece<false>(capturedPieceSq, enemy, toPieceRole);
        }
    }

    if (fromPieceRole == KING) board.kingSq[turn] = from;
}

void Chess::makeNull() {
    Square epsq = getEnPassant();

    state.push_back(State(state[ply], Move()));
    turn = ~turn;
    ++ply;

    state[ply].zkey ^= Zobrist::stm;
    if (epsq != INVALID) {
        state[ply].zkey ^= Zobrist::ep[Types::getFile(epsq)];
    }

    updateState();
}

void Chess::unmmakeNull() {
    --ply;
    turn = ~turn;
    state.pop_back();
}

template <bool forward>
void Chess::makeCastle(Square from, Square to, Color c) {
    if (forward)
        movePiece<forward>(from, to, c, KING);
    else
        movePiece<forward>(to, from, c, KING);

    if (to > from) {
        to = Square(from + 1);
        from = G::RookOriginOO[c];
    } else {
        to = Square(from - 1);
        from = G::RookOriginOOO[c];
    }

    if (forward)
        movePiece<forward>(from, to, c, ROOK);
    else
        movePiece<forward>(to, from, c, ROOK);
}

// Determine if a move is legal for the current board
bool Chess::isLegalMove(Move mv) const {
    Square from = mv.from(), to = mv.to(), king = board.getKingSq(turn);

    if (from == king) {
        if (mv.type() == CASTLE)
            return true;
        else  // Check if destination sq is attacked by enemy
            return !board.attacksTo(
                to, ~turn, board.occupancy() ^ BB::set(from) ^ BB::set(to));
    } else if (mv.type() == ENPASSANT) {
        Square enemyPawn = Types::pawnMove<PawnMove::PUSH, false>(to, turn);
        U64 occ = (board.occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) |
                  BB::set(to);

        // Check if captured pawn was blocking check
        return !(BB::movesByPiece<BISHOP>(king, occ) &
                 board.diagonalSliders(~turn)) &&
               !(BB::movesByPiece<ROOK>(king, occ) &
                 board.straightSliders(~turn));
    } else {
        // Check if moved piece was pinned
        return !state.at(ply).pinnedPieces ||
               !(state.at(ply).pinnedPieces & BB::set(from)) ||
               BB::bitsInline(from, to) & BB::set(king);
    }
}

// Determine if a move gives check for the current board
bool Chess::isCheckingMove(Move mv) const {
    Square from = mv.from(), to = mv.to();
    PieceRole role = Types::getPieceRole(board.getPiece(from));

    // Check if destination+piece role directly attacks the king
    if (state[ply].checkingSquares[role] & BB::set(to)) {
        return true;
    }

    // Check if moved piece was blocking enemy king from attack
    Square king = board.getKingSq(~turn);
    if (state[ply].discoveredCheckers &&
        (state[ply].discoveredCheckers & BB::set(from)) &&
        !(BB::bitsInline(from, to) & BB::set(king))) {
        return true;
    }

    switch (mv.type()) {
        case NORMAL:
            return false;

        case PROMOTION: {
            // Check if a promotion attacks the enemy king
            U64 occ = board.occupancy() ^ BB::set(from);
            return BB::movesByPiece(to, mv.promPiece(), occ) & BB::set(king);
        }

        case ENPASSANT: {
            // Check if captured pawn was blocking enemy king from attack
            Square enemyPawn = Types::pawnMove<PawnMove::PUSH, false>(to, turn);
            U64 occ = (board.occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) |
                      BB::set(to);

            return ((BB::movesByPiece<BISHOP>(king, occ) &
                     board.diagonalSliders(turn)) ||
                    (BB::movesByPiece<ROOK>(king, occ) &
                     board.straightSliders(turn)));
        }

        case CASTLE: {
            // Check if rook's destination after castling attacks enemy king
            Square rookFrom, rookTo;

            if (to > from) {
                rookTo = Square(from + 1);
                rookFrom = G::RookOriginOO[turn];
            } else {
                rookTo = Square(from - 1);
                rookFrom = G::RookOriginOOO[turn];
            }

            U64 occ = (board.occupancy() ^ BB::set(from) ^ BB::set(rookFrom)) |
                      BB::set(to) | BB::set(rookTo);

            return BB::movesByPiece<ROOK>(rookTo, occ) & BB::set(king);
        }
        default:
            return false;
    }

    return false;
}

Chess::Chess(const std::string& fen)
    : state{{State()}}, board{Board(fen)}, ply{0}, moveCounter{0} {
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
        moveCounter =
            2 * (std::stoul(tokens.at(5)) - 1) + (turn == WHITE ? 0 : 1);
    }

    state.at(ply).zkey = calculateKey();
    updateState();
}

std::string Chess::toFEN() const {
    std::ostringstream oss;

    for (Rank rank = RANK8; rank >= RANK1; rank--) {
        int emptyCount = 0;
        for (File file = FILE1; file <= FILE8; file++) {
            Piece p = board.getPiece(Types::getSquare(file, rank));
            if (p != NO_PIECE) {
                if (emptyCount > 0) {
                    oss << emptyCount;
                    emptyCount = 0;
                }
                oss << p;
            } else
                ++emptyCount;
        }

        if (emptyCount > 0) {
            oss << emptyCount;
            emptyCount = 0;
        }
        if (rank != RANK1) oss << '/';
    }

    if (turn)
        oss << " w ";
    else
        oss << " b ";

    if (canCastle(WHITE) || canCastle(BLACK)) {
        if (canCastleOO(WHITE)) oss << "K";
        if (canCastleOOO(WHITE)) oss << "Q";
        if (canCastleOO(BLACK)) oss << "k";
        if (canCastleOOO(BLACK)) oss << "q";
    } else {
        oss << "-";
    }

    Square enpassant = getEnPassant();
    if (enpassant != INVALID) {
        oss << " " << enpassant << " ";
    } else {
        oss << " - ";
    }

    oss << +state.at(ply).hmClock << " " << (moveCounter / 2) + 1;

    return oss.str();
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
