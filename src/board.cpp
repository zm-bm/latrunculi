#include "board.hpp"

#include "fen.hpp"
#include "score.hpp"
#include "thread.hpp"

// Determine if a move is legal for the current board
bool Board::isLegalMove(Move mv) const {
    Square from = mv.from();
    Square to   = mv.to();
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
        U64 occ          = (occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) | BB::set(to);
        return !(BB::pieceMoves<BISHOP>(king, occ) & pieces<BISHOP, QUEEN>(~turn)) &&
               !(BB::pieceMoves<ROOK>(king, occ) & pieces<ROOK, QUEEN>(~turn));
    } else {
        U64 blockers = state.at(ply).blockers[turn];
        return (!blockers || !(blockers & BB::set(from))     // piece isn't blocker
                || BB::inlineBB(from, to) & BB::set(king));  // or piece still blocks
    }
}

// Determine if a move gives check for the current board
bool Board::isCheckingMove(Move mv) const {
    Square from         = mv.from();
    Square to           = mv.to();
    PieceType pieceType = pieceTypeOf(pieceOn(from));
    Color enemy         = ~turn;
    Square enemyKing    = kingSq(enemy);

    // Check if destination+piece type directly attacks the king
    if (state[ply].checks[pieceType] & BB::set(to)) {
        return true;
    }

    // Check if moved piece was pinned
    U64 blockers = state[ply].blockers[enemy];
    if (blockers && (blockers & BB::set(from)) && !(BB::inlineBB(from, to) & BB::set(enemyKing))) {
        return true;
    }

    switch (mv.type()) {
        case NORMAL: return false;

        case PROMOTION: {
            // Check if a promotion attacks the enemy king
            U64 occ = occupancy() ^ BB::set(from);
            return BB::pieceMoves(to, mv.promoPiece(), occ) & BB::set(enemyKing);
        }

        case ENPASSANT: {
            // Check if captured pawn was blocking enemy king from attack
            Square enemyPawn = pawnMove<PUSH, false>(to, turn);
            U64 occ          = (occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) | BB::set(to);
            return ((BB::pieceMoves<BISHOP>(enemyKing, occ) & pieces<BISHOP, QUEEN>(turn)) ||
                    (BB::pieceMoves<ROOK>(enemyKing, occ) & pieces<ROOK, QUEEN>(turn)));
        }

        case CASTLE: {
            // Check if rook's destination after castling attacks enemy king
            CastleDirection dir = to > from ? KINGSIDE : QUEENSIDE;
            Square rFrom        = RookOrigin[dir][turn];
            Square rTo          = RookDestination[dir][turn];
            U64 occ = occupancy() ^ BB::set(from) ^ BB::set(rFrom) ^ BB::set(to) ^ BB::set(rTo);
            return BB::pieceMoves<ROOK>(rTo, occ) & BB::set(enemyKing);
        }
        default: return false;
    }

    return false;
}

void Board::make(Move mv) {
    // Get basic information about the move
    Square from             = mv.from();
    Square to               = mv.to();
    Square captureSq        = to;
    Square enpassantSq      = enPassantSq();
    PieceType pieceType     = pieceTypeOn(from);
    PieceType captPieceType = pieceTypeOn(to);
    MoveType movetype       = mv.type();
    Color enemy             = ~turn;
    if (movetype == ENPASSANT) {
        captPieceType      = PAWN;
        captureSq          = pawnMove<PUSH, false>(to, turn);
        squares[captureSq] = Piece::NONE;
    }

    // Create new board state and push it onto board state stack
    state.push_back(State(state[ply], mv));

    // Increment counters
    ++ply;
    ++moveCounter;

    if (thread) {
        thread->currentDepth++;
        thread->nodeCount++;
    }

    // Handle piece capture
    state[ply].captured = captPieceType;
    if (captPieceType) {
        state[ply].hmClock = 0;
        removePiece<true>(captureSq, enemy, captPieceType);

        // Disable castle rights if captured piece is rook
        if (canCastle(enemy) && captPieceType == ROOK) {
            disableCastle(enemy, captureSq);
        }
    }

    // Remove ep square from zobrist key if any
    if (enpassantSq != INVALID) {
        state[ply].zkey ^= Zobrist::ep[fileOf(enpassantSq)];
    }

    // Move the piece
    if (movetype == CASTLE) {
        movePiece<true>(from, to, turn, KING);
        CastleDirection dir = to > from ? KINGSIDE : QUEENSIDE;
        movePiece<true>(RookOrigin[dir][turn], RookDestination[dir][turn], turn, ROOK);
    } else {
        movePiece<true>(from, to, turn, pieceType);
    }

    switch (pieceType) {
        // handle enpassants and promotions
        case PAWN: {
            state[ply].hmClock = 0;
            if (to == from + DOUBLE || to == from - DOUBLE) {
                Square sq                  = pawnMove<PUSH, false>(to, turn);
                state.at(ply).enPassantSq  = sq;
                state.at(ply).zkey        ^= Zobrist::ep[fileOf(sq)];
            } else if (movetype == PROMOTION) {
                removePiece<true>(to, turn, PAWN);
                addPiece<true>(to, turn, mv.promoPiece());
            }
            break;
        }

        // disable castling
        case KING: {
            kingSquare[turn] = to;
            if (canCastle(turn)) disableCastle(turn);
            break;
        }
        case ROOK: {
            if (canCastle(turn)) disableCastle(turn, from);
            break;
        }

        default: break;
    }

    turn             = enemy;
    state[ply].zkey ^= Zobrist::stm;

    updateCheckInfo();
}

void Board::unmake() {
    // Get basic move information
    Color enemy             = turn;
    Move mv                 = state[ply].move;
    Square from             = mv.from();
    Square to               = mv.to();
    PieceType captPieceType = state[ply].captured;
    PieceType pieceType     = pieceTypeOn(to);
    MoveType movetype       = mv.type();

    // Decrement counters
    if (thread) thread->currentDepth--;
    --ply;
    --moveCounter;

    // Revert to the previous board state
    turn = ~turn;
    state.pop_back();

    // Make corrections if promotion move
    if (movetype == PROMOTION) {
        removePiece<false>(to, turn, pieceType);
        addPiece<false>(to, turn, PAWN);
        pieceType = PAWN;
    }

    // Undo the move
    if (movetype == CASTLE) {
        movePiece<false>(to, from, turn, KING);
        CastleDirection dir = to > from ? KINGSIDE : QUEENSIDE;
        movePiece<false>(RookDestination[dir][turn], RookOrigin[dir][turn], turn, ROOK);
    } else {
        movePiece<false>(to, from, turn, pieceType);
        if (captPieceType) {
            Square captureSq = (movetype == ENPASSANT) ? pawnMove<PUSH, false>(to, turn) : to;
            addPiece<false>(captureSq, enemy, captPieceType);
        }
    }

    if (pieceType == KING) {
        kingSquare[turn] = from;
    }
}

void Board::makeNull() {
    Square enpassantSq = enPassantSq();

    state.push_back(State(state[ply], Move()));
    turn = ~turn;
    ++ply;

    state[ply].zkey ^= Zobrist::stm;
    if (enpassantSq != INVALID) {
        state[ply].zkey ^= Zobrist::ep[fileOf(enpassantSq)];
    }

    updateCheckInfo();
}

void Board::unmmakeNull() {
    --ply;
    turn = ~turn;
    state.pop_back();
}

void Board::loadFEN(const std::string& fen) {
    FenParser parser(fen);

    auto pieces = parser.pieces;
    for (auto p = pieces.begin(); p != pieces.end(); ++p) {
        addPiece<true>(p->square, p->color, p->type);
        if (p->type == KING) kingSquare[p->color] = p->square;
    }

    turn                      = parser.turn;
    state.at(ply).castle      = parser.castle;
    state.at(ply).enPassantSq = parser.enPassantSq;
    state.at(ply).hmClock     = parser.hmClock;
    moveCounter               = parser.moveCounter;

    state.at(ply).zkey = calculateKey();
    updateCheckInfo();
}

std::string Board::toFEN() const {
    std::ostringstream oss;

    for (Rank rank = RANK8; rank >= RANK1; rank--) {
        int emptyCount = 0;
        for (File file = FILE1; file <= FILE8; file++) {
            Piece p = pieceOn(file, rank);
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

    if (canCastle(WHITE) || canCastle(BLACK)) {
        if (canCastleOO(WHITE)) oss << "K";
        if (canCastleOOO(WHITE)) oss << "Q";
        if (canCastleOO(BLACK)) oss << "k";
        if (canCastleOOO(BLACK)) oss << "q";
    } else {
        oss << "-";
    }

    Square enpassantSq = enPassantSq();
    if (enpassantSq != INVALID) {
        oss << " " << enpassantSq << " ";
    } else {
        oss << " - ";
    }

    oss << +state.at(ply).hmClock << " " << (moveCounter / 2) + 1;

    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const Board& chess) {
    for (Rank rank = RANK8; rank >= RANK1; rank--) {
        os << "   +---+---+---+---+---+---+---+---+\n";
        os << "   |";
        for (File file = FILE1; file <= FILE8; file++) {
            os << " " << chess.pieceOn(file, rank) << " |";
        }
        os << " " << rank << '\n';
    }

    os << "   +---+---+---+---+---+---+---+---+\n";
    os << "     a   b   c   d   e   f   g   h\n\n";
    os << chess.toFEN() << std::endl;

    return os;
}
