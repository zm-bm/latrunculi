#include "board.hpp"

#include "fen.hpp"
#include "movegen.hpp"
#include "score.hpp"
#include "thread.hpp"

int Board::see(Move move) const {
    Square from         = move.from();
    Square to           = move.to();
    PieceType fromPiece = pieceTypeOn(from);
    PieceType toPiece   = pieceTypeOn(to);
    Color side          = sideToMove();

    int gain[32] = {};
    int depth    = 0;
    gain[depth]  = pieceValue(toPiece);

    U64 occupied  = occupancy();
    U64 attackers = attacksTo(to, occupied);
    U64 fromBB    = BB::set(from);

    do {
        depth++;
        side         = ~side;
        gain[depth]  = pieceValue(fromPiece) - gain[depth - 1];
        occupied    ^= fromBB;
        attackers    = attacksTo(to, occupied) & occupied;

        // get least valuable attacker
        U64 sideAttackers = attackers & pieces<PieceType::All>(side);
        int least         = INT_MAX;
        fromBB            = 0;
        while (sideAttackers) {
            Square sq = BB::lsbPop(sideAttackers);
            if (pieceValue(pieceTypeOn(sq)) < least) {
                fromBB    = BB::set(sq);
                fromPiece = pieceTypeOn(sq);
            }
        }
    } while (fromBB);

    while (--depth) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
    }

    return gain[0];
}

// Determine if board is in a drawn position
bool Board::isDraw() const {
    MoveGenerator<MoveGenMode::All> moves{*this};
    for (auto& move : moves) {
        if (isLegalMove(move)) return false;
    }
    return true;
}

// Determine if a move is legal for the current board
bool Board::isLegalMove(Move mv) const {
    Square from = mv.from();
    Square to   = mv.to();
    Square king = kingSq(turn);

    if (from == king) {
        if (mv.type() == MoveType::Castle) {
            return true;
        } else {
            // Check if destination sq is attacked by enemy
            U64 occupied = occupancy() ^ BB::set(from) ^ BB::set(to);
            return !attacksTo(to, ~turn, occupied);
        }
    } else if (mv.type() == MoveType::EnPassant) {
        // Check if captured pawn was blocking check
        Square enemyPawn = pawnMove<PawnMove::Push, BACKWARD>(to, turn);
        U64 occupied     = (occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) | BB::set(to);
        auto diagSliders = pieces<PieceType::Bishop, PieceType::Queen>(~turn);
        auto lineSliders = pieces<PieceType::Rook, PieceType::Queen>(~turn);
        return !(diagSliders & BB::moves<PieceType::Bishop>(king, occupied)) &&
               !(lineSliders & BB::moves<PieceType::Rook>(king, occupied));
    } else {
        U64 blockers = state.at(ply).blockers[turn];
        return (!blockers || !(blockers & BB::set(from))      // piece isn't blocker
                || BB::collinear(from, to) & BB::set(king));  // or piece still blocks
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
    if (state[ply].checks[idx(pieceType)] & BB::set(to)) {
        return true;
    }

    // Check if moved piece was pinned
    U64 blockers = state[ply].blockers[enemy];
    if (blockers && (blockers & BB::set(from)) && !(BB::collinear(from, to) & BB::set(enemyKing))) {
        return true;
    }

    switch (mv.type()) {
        case MoveType::Normal: return false;

        case MoveType::Promotion: {
            // Check if a promotion attacks the enemy king
            U64 occupied = occupancy() ^ BB::set(from);
            return BB::moves(to, mv.promoPiece(), occupied) & BB::set(enemyKing);
        }

        case MoveType::EnPassant: {
            // Check if captured pawn was blocking enemy king from attack
            Square enemyPawn = pawnMove<PawnMove::Push, BACKWARD>(to, turn);
            U64 occupied     = (occupancy() ^ BB::set(from) ^ BB::set(enemyPawn)) | BB::set(to);
            auto diagSliders = pieces<PieceType::Bishop, PieceType::Queen>(turn);
            auto lineSliders = pieces<PieceType::Rook, PieceType::Queen>(turn);
            return ((diagSliders & BB::moves<PieceType::Bishop>(enemyKing, occupied)) ||
                    (lineSliders & BB::moves<PieceType::Rook>(enemyKing, occupied)));
        }

        case MoveType::Castle: {
            // Check if rook's destination after castling attacks enemy king
            auto dir        = to > from ? idx(Castle::KingSide) : idx(Castle::QueenSide);
            Square rookFrom = RookOrigin[dir][turn];
            Square rookTo   = RookDestination[dir][turn];
            U64 occupied    = occupancy() ^ BB::set(from, rookFrom, to, rookTo);
            return BB::moves<PieceType::Rook>(rookTo, occupied) & BB::set(enemyKing);
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
    if (movetype == MoveType::EnPassant) {
        captPieceType      = PieceType::Pawn;
        captureSq          = pawnMove<PawnMove::Push, BACKWARD>(to, turn);
        squares[captureSq] = Piece::None;
    }

    // Create new board state and push it onto board state stack
    state.push_back(State(state[ply], mv));

    // Increment counters
    ++ply;
    ++moveCounter;

    if (thread) thread->ply++;

    // Handle piece capture
    state[ply].captured = captPieceType;
    if (captPieceType != PieceType::None) {
        state[ply].hmClock = 0;
        removePiece<true>(captureSq, enemy, captPieceType);

        // Disable castle rights if captured piece is rook
        if (canCastle(enemy) && captPieceType == PieceType::Rook) {
            disableCastle(enemy, captureSq);
        }
    }

    // Remove ep square from zobrist key if any
    if (enpassantSq != INVALID) {
        state[ply].zkey ^= Zobrist::hashEp(enpassantSq);
    }

    // Move the piece
    if (movetype == MoveType::Castle) {
        movePiece<true>(from, to, turn, PieceType::King);
        auto dir = to > from ? idx(Castle::KingSide) : idx(Castle::QueenSide);
        movePiece<true>(RookOrigin[dir][turn], RookDestination[dir][turn], turn, PieceType::Rook);
    } else {
        movePiece<true>(from, to, turn, pieceType);
    }

    switch (pieceType) {
        // handle enpassants and promotions
        case PieceType::Pawn: {
            state[ply].hmClock = 0;
            if (std::abs(to - from) == idx(PawnMove::Double)) {
                Square sq                  = pawnMove<PawnMove::Push, BACKWARD>(to, turn);
                state.at(ply).enPassantSq  = sq;
                state.at(ply).zkey        ^= Zobrist::hashEp(sq);
            } else if (movetype == MoveType::Promotion) {
                removePiece<true>(to, turn, PieceType::Pawn);
                addPiece<true>(to, turn, mv.promoPiece());
            }
            break;
        }

        // disable castling
        case PieceType::King: {
            kingSquare[turn] = to;
            if (canCastle(turn)) disableCastle(turn);
            break;
        }
        case PieceType::Rook: {
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
    if (thread) thread->ply--;
    --ply;
    --moveCounter;

    // Revert to the previous board state
    turn = ~turn;
    state.pop_back();

    // Make corrections if promotion move
    if (movetype == MoveType::Promotion) {
        removePiece<false>(to, turn, pieceType);
        addPiece<false>(to, turn, PieceType::Pawn);
        pieceType = PieceType::Pawn;
    }

    // Undo the move
    if (movetype == MoveType::Castle) {
        movePiece<false>(to, from, turn, PieceType::King);
        auto dir = to > from ? idx(Castle::KingSide) : idx(Castle::QueenSide);
        movePiece<false>(RookDestination[dir][turn], RookOrigin[dir][turn], turn, PieceType::Rook);
    } else {
        movePiece<false>(to, from, turn, pieceType);
        if (captPieceType != PieceType::None) {
            Square captureSq = (movetype != MoveType::EnPassant)
                                   ? to
                                   : pawnMove<PawnMove::Push, BACKWARD>(to, turn);
            addPiece<false>(captureSq, enemy, captPieceType);
        }
    }

    if (pieceType == PieceType::King) {
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
        state[ply].zkey ^= Zobrist::hashEp(enpassantSq);
    }

    updateCheckInfo();
}

void Board::unmmakeNull() {
    --ply;
    turn = ~turn;
    state.pop_back();
}

void Board::loadFEN(const std::string& fen) {
    for (size_t c = 0; c < N_COLORS; ++c) {
        for (size_t p = 0; p < N_PIECES; ++p) {
            piecesBB[c][p]   = 0;
            pieceCount[c][p] = 0;
        }
    }

    for (size_t sq = 0; sq < N_SQUARES; ++sq) {
        squares[sq] = Piece::None;
    }

    material = {0, 0};
    psqBonus = {0, 0};
    state    = {State()};
    ply      = 0;

    FenParser parser(fen);
    for (const auto p : parser.pieces) {
        addPiece<true>(p.square, p.color, p.type);
        if (p.type == PieceType::King) kingSquare[p.color] = p.square;
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

    for (Rank rank = Rank::R8; rank >= Rank::R1; --rank) {
        int emptyCount = 0;
        for (File file = File::F1; file <= File::F8; ++file) {
            Piece p = pieceOn(file, rank);
            if (p != Piece::None) {
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
        if (rank != Rank::R1) oss << '/';
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

std::string Board::toSAN(Move move) const {
    std::string result  = "";
    Square from         = move.from();
    Square to           = move.to();
    PieceType pieceType = pieceTypeOn(from);

    if (move.type() == MoveType::Castle) {
        if (move.from() < move.to())
            return "O-O";
        else
            return "O-O-O";
    }

    if (pieceType != PieceType::Pawn) result += std::toupper(toChar(pieceType));

    MoveGenerator<MoveGenMode::All> moves{*this};
    auto ambigMove = std::find_if(moves.begin(), moves.end(), [&](Move m) {
        return m.to() == to && pieceTypeOn(m.from()) == pieceType && m.from() != from &&
               isLegalMove(m);
    });
    if (ambigMove != moves.end()) {
        bool diffFile = fileOf(ambigMove->from()) != fileOf(from);
        bool diffRank = rankOf(ambigMove->from()) != rankOf(from);

        if (diffFile)
            result += toChar(fileOf(from));
        else if (diffRank)
            result += toChar(rankOf(from));
    }

    if (isCapture(move)) {
        if (pieceType == PieceType::Pawn) result += toChar(fileOf(from));
        result += 'x';
    }

    result += toString(to);
    if (move.type() == MoveType::Promotion) result += '=' + toChar(move.promoPiece());
    if (isCheckingMove(move)) result += '+';

    return result;
}

std::ostream& operator<<(std::ostream& os, const Board& board) {
    for (Rank rank = Rank::R8; rank >= Rank::R1; --rank) {
        os << "   +---+---+---+---+---+---+---+---+\n";
        os << "   |";
        for (File file = File::F1; file <= File::F8; ++file) {
            os << " " << board.pieceOn(file, rank) << " |";
        }
        os << " " << rank << '\n';
    }

    os << "   +---+---+---+---+---+---+---+---+\n";
    os << "     a   b   c   d   e   f   g   h\n\n";
    os << board.toFEN() << std::endl;

    return os;
}

template <NodeType node>
U64 Board::perft(int depth, std::ostream& oss) {
    if (depth == 0) return 1;

    MoveGenerator<MoveGenMode::All> moves{*this};

    U64 count = 0, nodes = 0;

    for (auto& move : moves) {
        if (!isLegalMove(move)) continue;

        make(move);

        count  = perft<NodeType::NonPV>(depth - 1);
        nodes += count;

        if constexpr (node == NodeType::Root) {
            oss << move << ": " << count << '\n';
        }

        unmake();
    }

    if constexpr (node == NodeType::Root) {
        oss << "NODES: " << nodes << std::endl;
    }

    return nodes;
}
template U64 Board::perft<NodeType::Root>(int, std::ostream& = std::cout);
