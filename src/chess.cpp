#include "chess.hpp"

#include "evaluator.hpp"
#include "fen.hpp"
#include "score.hpp"
#include "thread.hpp"

Chess::Chess(const std::string& fen, Thread* thread) : thread(thread) {
    loadFEN(fen);
}

void Chess::loadFEN(const std::string& fen) {
    FenParser parser(fen);

    auto pieces = parser.pieces;
    for (auto p = pieces.begin(); p != pieces.end(); ++p) {
        addPiece<true>(p->square, p->color, p->type);
        if (p->type == KING) kingSquare[p->color] = p->square;
    }

    turn = parser.turn;
    state.at(ply).castle = parser.castle;
    state.at(ply).enPassantSq = parser.enPassantSq;
    state.at(ply).hmClock = parser.hmClock;
    moveCounter = parser.moveCounter;

    state.at(ply).zkey = calculateKey();
    updateState();
}

template <bool debug = false>
int Chess::eval() const {
    return Evaluator<debug>(*this).eval();
}

template int Chess::eval<true>() const;
template int Chess::eval<false>() const;

void Chess::make(Move mv) {
    // Get basic information about the move
    Square from = mv.from();
    Square to = mv.to();
    Square capturedPieceSq = to;
    Square epsq = getEnPassant();
    PieceType fromPieceType = pieceTypeOn(from);
    PieceType toPieceType = pieceTypeOn(to);
    MoveType movetype = mv.type();
    Color enemy = ~turn;
    if (movetype == ENPASSANT) {
        toPieceType = PAWN;
        capturedPieceSq = pawnMove<PUSH, false>(to, turn);
        squares[capturedPieceSq] = Piece::NONE;
    }

    // Create new board state and push it onto board state stack
    state.push_back(State(state[ply], mv));
    ++ply;
    ++moveCounter;
    if (thread) thread->currentDepth++;

    // Store the captured piece type for undoing move
    state[ply].captured = toPieceType;
    if (toPieceType) {
        handlePieceCapture(capturedPieceSq, enemy, toPieceType);
    }

    // Remove ep square from zobrist key if any
    if (epsq != INVALID) {
        state[ply].zkey ^= Zobrist::ep[fileOf(epsq)];
    }

    // Move the piece
    if (movetype == CASTLE) {
        moveCastle<true>(from, to, turn);
    } else {
        movePiece<true>(from, to, turn, fromPieceType);
    }

    // Handle en passants, promotions, and update castling rights
    switch (fromPieceType) {
        case PAWN: {
            handlePawnMoves(from, to, movetype, mv);
            break;
        }

        case KING: {
            kingSquare[turn] = to;
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

    updateState();
}

void Chess::unmake() {
    // Get basic move information
    Color enemy = turn;
    Move mv = state[ply].move;
    Square from = mv.from(), to = mv.to();
    PieceType toPieceType = state[ply].captured;
    PieceType fromPieceType = pieceTypeOf(pieceOn(to));
    MoveType movetype = mv.type();

    // Revert to the previous board state
    if (thread) thread->currentDepth--;
    --ply;
    --moveCounter;
    turn = ~turn;
    state.pop_back();

    // Make corrections if promotion move
    if (movetype == PROMOTION) {
        removePiece<false>(to, turn, fromPieceType);
        addPiece<false>(to, turn, PAWN);
        fromPieceType = PAWN;
    }

    // Undo the move
    if (movetype == CASTLE) {
        moveCastle<false>(from, to, turn);
    } else {
        movePiece<false>(to, from, turn, fromPieceType);
        if (toPieceType) {
            Square capturedPieceSq = (movetype == ENPASSANT) ? pawnMove<PUSH, false>(to, turn) : to;
            addPiece<false>(capturedPieceSq, enemy, toPieceType);
        }
    }

    if (fromPieceType == KING) {
        kingSquare[turn] = from;
    }
}

void Chess::makeNull() {
    Square epsq = getEnPassant();

    state.push_back(State(state[ply], Move()));
    turn = ~turn;
    ++ply;

    state[ply].zkey ^= Zobrist::stm;
    if (epsq != INVALID) {
        state[ply].zkey ^= Zobrist::ep[fileOf(epsq)];
    }

    updateState();
}

void Chess::unmmakeNull() {
    --ply;
    turn = ~turn;
    state.pop_back();
}

template <bool forward>
void Chess::moveCastle(Square from, Square to, Color c) {
    if constexpr (forward) {
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

    if constexpr (forward) {
        movePiece<forward>(from, to, c, ROOK);
    } else {
        movePiece<forward>(to, from, c, ROOK);
    }
}

// Determine if a move is legal for the current board
bool Chess::isLegalMove(Move mv) const {
    Square from = mv.from();
    Square to = mv.to();
    Square king = kingSq(turn);

    if (from == king) {
        if (mv.type() == CASTLE) {
            return true;
        } else {
            // Check if destination sq is attacked by enemy
            U64 occ = occupancy() ^ BB::set(from) ^ BB::set(to);
            return !attacksTo(to, ~turn, occ);
        }
    } else if (mv.type() == ENPASSANT) {
        // Check if captured pawn was blocking check
        Square enemyPawn = pawnMove<PUSH, false>(to, turn);
        U64 occ = (occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) | BB::set(to);
        return !(BB::pieceMoves<BISHOP>(king, occ) & pieces<BISHOP, QUEEN>(~turn)) &&
               !(BB::pieceMoves<ROOK>(king, occ) & pieces<ROOK, QUEEN>(~turn));
    } else {
        // Check if moved piece was pinned
        return !state.at(ply).blockers[turn] || !(state.at(ply).blockers[turn] & BB::set(from)) ||
               BB::inlineBB(from, to) & BB::set(king);
    }
}

// Determine if a move gives check for the current board
bool Chess::isCheckingMove(Move mv) const {
    Square from = mv.from(), to = mv.to();
    PieceType pieceType = pieceTypeOf(pieceOn(from));

    // Check if destination+piece type directly attacks the king
    if (state[ply].checkingSquares[pieceType] & BB::set(to)) {
        return true;
    }

    // Check if moved piece was blocking enemy king from attack
    Square king = kingSq(~turn);
    U64 pinners = state[ply].pinners[turn];
    if (pinners && (pinners & BB::set(from)) && !(BB::inlineBB(from, to) & BB::set(king))) {
        return true;
    }

    switch (mv.type()) {
        case NORMAL: return false;

        case PROMOTION: {
            // Check if a promotion attacks the enemy king
            U64 occ = occupancy() ^ BB::set(from);
            return BB::pieceMoves(to, mv.promoPiece(), occ) & BB::set(king);
        }

        case ENPASSANT: {
            // Check if captured pawn was blocking enemy king from attack
            Square enemyPawn = pawnMove<PUSH, false>(to, turn);
            U64 occ = (occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) | BB::set(to);
            return ((BB::pieceMoves<BISHOP>(king, occ) & pieces<BISHOP, QUEEN>(turn)) ||
                    (BB::pieceMoves<ROOK>(king, occ) & pieces<ROOK, QUEEN>(turn)));
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
                (occupancy() ^ BB::set(from) ^ BB::set(rFrom)) | BB::set(to) | BB::set(rTo);
            return BB::pieceMoves<ROOK>(rTo, occ) & BB::set(king);
        }
        default: return false;
    }

    return false;
}



std::string Chess::toFEN() const {
    std::ostringstream oss;

    for (Rank rank = RANK8; rank >= RANK1; rank--) {
        int emptyCount = 0;
        for (File file = FILE1; file <= FILE8; file++) {
            Piece p = pieceOn(makeSquare(file, rank));
            if (p != Piece::NONE) {
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
    for (Rank rank = RANK8; rank >= RANK1; rank--) {
        os << "   +---+---+---+---+---+---+---+---+\n";
        os << "   |";
        for (File file = FILE1; file <= FILE8; file++) {
            os << " " << chess.pieceOn(makeSquare(file, rank)) << " |";
        }
        os << " " << rank << '\n';
    }

    os << "   +---+---+---+---+---+---+---+---+\n";
    os << "     a   b   c   d   e   f   g   h\n\n";
    os << chess.toFEN() << std::endl;

    return os;
}
