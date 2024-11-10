#include "chess.hpp"

#include "defs.hpp"
#include "eval.hpp"
#include "fen.hpp"

/*
    EVAL
*/

template <bool debug>
int Chess::eval() const {
    int score = 0;

    score += materialScore;          // piece values
    score += evalPosition<debug>();  // piece+square values

    // score relative to side to move
    score *= ((2 * turn) - 1);

    if (debug) {
        std::cout << score << std::endl;
    }

    return score;
}

template int Chess::eval<true>() const;
template int Chess::eval<false>() const;

template <bool debug>
int Chess::evalPosition() const {
    int score = 0;

    return score;
}

/*
    MOVE GENERATION
*/

void Chess::make(Move mv) {
    // Get basic information about the move
    bool checkingMove = isCheckingMove(mv);
    Square from = mv.from();
    Square to = mv.to();
    Square capturedPieceSq = to;
    Square epsq = getEnPassant();
    PieceRole fromPieceRole = Defs::getPieceRole(board.getPiece(from));
    PieceRole toPieceRole = Defs::getPieceRole(board.getPiece(to));
    MoveType movetype = mv.type();
    Color enemy = ~turn;
    if (movetype == ENPASSANT) {
        toPieceRole = PAWN;
        capturedPieceSq = Defs::pawnMove<PawnMove::PUSH, false>(to, turn);
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
        state[ply].zkey ^= Zobrist::ep[Defs::fileFromSq(epsq)];
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
            if (state.at(ply).canCastle(turn)) {
                state.at(ply).disableCastle(turn);
            }
            break;
        }

        case ROOK: {
            if (state.at(ply).canCastle(turn)) {
                state.at(ply).disableCastle(turn, from);
            }
            break;
        }

        default: break;
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
    PieceRole fromPieceRole = Defs::getPieceRole(board.getPiece(to));
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
            Square capturedPieceSq =
                (movetype == ENPASSANT) ? Defs::pawnMove<PawnMove::PUSH, false>(to, turn) : to;
            addPiece<false>(capturedPieceSq, enemy, toPieceRole);
        }
    }

    if (fromPieceRole == KING) {
        board.kingSq[turn] = from;
    }
}

void Chess::makeNull() {
    Square epsq = getEnPassant();

    state.push_back(State(state[ply], Move()));
    turn = ~turn;
    ++ply;

    state[ply].zkey ^= Zobrist::stm;
    if (epsq != INVALID) {
        state[ply].zkey ^= Zobrist::ep[Defs::fileFromSq(epsq)];
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
    if (forward) {
        movePiece<forward>(from, to, c, KING);
    } else {
        movePiece<forward>(to, from, c, KING);
    }

    if (to > from) {
        to = Square(from + 1);
        from = RookOriginOO[c];
    } else {
        to = Square(from - 1);
        from = RookOriginOOO[c];
    }

    if (forward) {
        movePiece<forward>(from, to, c, ROOK);
    } else {
        movePiece<forward>(to, from, c, ROOK);
    }
}

// Determine if a move is legal for the current board
bool Chess::isPseudoLegalMoveLegal(Move mv) const {
    Square from = mv.from();
    Square to = mv.to();
    Square king = board.getKingSq(turn);

    if (from == king) {
        if (mv.type() == CASTLE) {
            return true;
        } else {
            // Check if destination sq is attacked by enemy
            U64 occ = board.occupancy() ^ BB::set(from) ^ BB::set(to);
            return !board.attacksTo(to, ~turn, occ);
        }
    } else if (mv.type() == ENPASSANT) {
        // Check if captured pawn was blocking check
        Square enemyPawn = Defs::pawnMove<PawnMove::PUSH, false>(to, turn);
        U64 occ = (board.occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) | BB::set(to);
        return !(BB::movesByPiece<BISHOP>(king, occ) & board.diagonalSliders(~turn)) &&
               !(BB::movesByPiece<ROOK>(king, occ) & board.straightSliders(~turn));
    } else {
        // Check if moved piece was pinned
        return !state.at(ply).pinnedPieces || !(state.at(ply).pinnedPieces & BB::set(from)) ||
               BB::bitsInline(from, to) & BB::set(king);
    }
}

// Determine if a move gives check for the current board
bool Chess::isCheckingMove(Move mv) const {
    Square from = mv.from(), to = mv.to();
    PieceRole role = Defs::getPieceRole(board.getPiece(from));

    // Check if destination+piece role directly attacks the king
    if (state[ply].checkingSquares[role] & BB::set(to)) {
        return true;
    }

    // Check if moved piece was blocking enemy king from attack
    Square king = board.getKingSq(~turn);
    if (state[ply].discoveredCheckers && (state[ply].discoveredCheckers & BB::set(from)) &&
        !(BB::bitsInline(from, to) & BB::set(king))) {
        return true;
    }

    switch (mv.type()) {
        case NORMAL: return false;

        case PROMOTION: {
            // Check if a promotion attacks the enemy king
            U64 occ = board.occupancy() ^ BB::set(from);
            return BB::movesByPiece(to, mv.promoPiece(), occ) & BB::set(king);
        }

        case ENPASSANT: {
            // Check if captured pawn was blocking enemy king from attack
            Square enemyPawn = Defs::pawnMove<PawnMove::PUSH, false>(to, turn);
            U64 occ = (board.occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) | BB::set(to);
            return ((BB::movesByPiece<BISHOP>(king, occ) & board.diagonalSliders(turn)) ||
                    (BB::movesByPiece<ROOK>(king, occ) & board.straightSliders(turn)));
        }

        case CASTLE: {
            // Check if rook's destination after castling attacks enemy king
            Square rFrom, rTo;
            if (to > from) {
                rTo = Square(from + 1);
                rFrom = RookOriginOO[turn];
            } else {
                rTo = Square(from - 1);
                rFrom = RookOriginOOO[turn];
            }

            U64 occ =
                (board.occupancy() ^ BB::set(from) ^ BB::set(rFrom)) | BB::set(to) | BB::set(rTo);
            return BB::movesByPiece<ROOK>(rTo, occ) & BB::set(king);
        }
        default: return false;
    }

    return false;
}

Chess::Chess(const std::string& fen) : state{{State()}}, board{Board()}, ply{0}, moveCounter{0} {
    FenParser parser(fen);

    auto piece_placement = parser.getPiecePlacement();
    for (auto piece = piece_placement.begin(); piece != piece_placement.end(); ++piece) {
        addPiece<true>(piece->square, piece->color, piece->role);

        if (piece->role == KING) {
            board.kingSq[piece->color] = piece->square;
        }
    }

    turn = parser.getActiveColor();
    state.at(ply).castle = parser.getCastlingRights();
    state.at(ply).enPassantSq = parser.getEnPassantTarget();
    state.at(ply).hmClock = parser.getHalfmoveClock();
    moveCounter = parser.getFullmoveNumber();
    
    state.at(ply).zkey = calculateKey();
    updateState();
}

std::string Chess::toFEN() const {
    std::ostringstream oss;

    for (Rank rank = RANK8; rank >= RANK1; rank--) {
        int emptyCount = 0;
        for (File file = FILE1; file <= FILE8; file++) {
            Piece p = board.getPiece(Defs::sqFromCoords(file, rank));
            if (p != NO_PIECE) {
                if (emptyCount > 0) {
                    oss << emptyCount;
                    emptyCount = 0;
                }
                oss << p;
            } else {
                ++emptyCount;
            }
        }

        if (emptyCount > 0) {
            oss << emptyCount;
            emptyCount = 0;
        }
        if (rank != RANK1) oss << '/';
    }

    if (turn) {
        oss << " w ";
    } else {
        oss << " b ";
    }

    if (state.at(ply).canCastle(WHITE) || state.at(ply).canCastle(BLACK)) {
        if (state.at(ply).canCastleOO(WHITE)) oss << "K";
        if (state.at(ply).canCastleOOO(WHITE)) oss << "Q";
        if (state.at(ply).canCastleOO(BLACK)) oss << "k";
        if (state.at(ply).canCastleOOO(BLACK)) oss << "q";
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

    return os;
}
