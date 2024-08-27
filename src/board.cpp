#include "board.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "types.hpp"
#include "movegen.hpp"

// Create a board using a FEN string
// See: https://www.chessprogramming.org/Forsyth-Edwards_Notation
Board::Board(const std::string& fen)
    : state{ {State()} }
    , ply{0}
    , fullMoveCounter{0}
    , openingScore{0}
    , endgameScore{0}
    , materialScore{0}
{
    for (int i = 0; i < 7; i++) {
        pieces[BLACK][i] = BBz(0x0UL);
        pieces[WHITE][i] = BBz(0x0UL);
        pieceCount[BLACK][i] = 0;
        pieceCount[WHITE][i] = 0;
    }
    for (Square sq = A1; sq < INVALID; sq++)
        squares[sq] = EMPTY;

    loadFEN(fen);
    updateState();
}

// Determine if a move is legal for the current board
bool Board::isLegalMove(Move mv) const
{
    Square from = mv.from(),
           to = mv.to(),
           king = getKingSq(stm);

    if (from == king)
    {
        if (mv.type() == CASTLE)
            return true;
        else  // Check if destination sq is attacked by enemy
            return !attacksTo(to, ~stm, occupancy() ^ from ^ to);
    }
    else if (mv.type() == ENPASSANT)
    {
        Square enemyPawn = Types::pawnMove<PawnMove::PUSH, false>(to, stm);
        BBz occ = (occupancy() ^ from ^ enemyPawn) | to;

        // Check if captured pawn was blocking check
        return !(MoveGen::movesByPiece<BISHOP>(king, occ) & diagonalSliders(~stm))
            && !(MoveGen::movesByPiece<ROOK  >(king, occ) & straightSliders(~stm));
    }
    else
    {
        // Check if moved piece was pinned
        return !state.at(ply).pinnedPieces
            || !(state.at(ply).pinnedPieces & from)
            || G::BITS_INLINE[from][to] & G::BITSET[king];
    }
}

// Determine if a move gives check for the current board
bool Board::isCheckingMove(Move mv) const
{
    Square from = mv.from(),
           to = mv.to();
    PieceType pieceType = Types::getPieceType(getPiece(from));

    // Check if destination+piece type directly attacks the king
    if (state[ply].checkingSquares[pieceType] & to)
        return true;

    // Check if moved piece was blocking enemy king from attack
    Square king = getKingSq(~stm);
    if (state[ply].discoveredCheckers
        && (state[ply].discoveredCheckers & from)
        && !(G::BITS_INLINE[from][to] & G::BITSET[king]))
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
            BBz occ = occupancy() ^ from;
            return MoveGen::movesByPiece(to, mv.promPiece(), occ) & king;
        }

        case ENPASSANT:
        {
            // Check if captured pawn was blocking enemy king from attack
            Square enemyPawn = Types::pawnMove<PawnMove::PUSH, false>(to, stm);
            BBz occ = (occupancy() ^ from ^ enemyPawn) | to;

            return MoveGen::movesByPiece<BISHOP>(king, occ) & diagonalSliders(stm)
                || MoveGen::movesByPiece<ROOK>(king, occ)   & straightSliders(stm);
        }

        case CASTLE:
        {
            // Check if rook's destination after castling attacks enemy king
            Square rookFrom, rookTo;

            if (to > from) {
                rookTo = Square(from + 1);
                rookFrom = MoveGen::RookOriginOO[stm];
            }
            else {
                rookTo = Square(from-1);
                rookFrom = MoveGen::RookOriginOOO[stm];
            }

            BBz occ = (occupancy() ^ from ^ rookFrom) | to | rookTo;

            return MoveGen::movesByPiece<ROOK>(rookTo, occ) & king;
        }
        default:
            return false;
    }

    return false;
}

// Determine pieces of color c, which block the color kingC from attack by the enemy
BBz Board::getCheckBlockers(Color c, Color kingC) const
{
    Color enemy = ~kingC;
    Square king = getKingSq(kingC);

    // Determine which enemy sliders could check the kingC's king
    BBz blockers = BBz(0x0),
       pinners = (MoveGen::movesByPiece<BISHOP>(king) & diagonalSliders(enemy))
               | (MoveGen::movesByPiece<ROOK  >(king) & straightSliders(enemy));

    while (pinners)
    {
        // For each potential pinning piece
        Square pinner = pinners.lsb();
        pinners.clear(pinner);

        // Check if only one piece separates the slider and the king
        BBz piecesInBetween = occupancy() & G::BITS_BETWEEN[king][pinner];
        if (!piecesInBetween.moreThanOneSet())
            blockers |= piecesInBetween & getPieces<ALL>(c);
    }

    return blockers;
}

int Board::calculateMobilityScore(const int opPhase, const int egPhase) const
{
    int score = 0;
    score += calculateMobilityScore<KNIGHT>(opPhase, egPhase);
    score += calculateMobilityScore<BISHOP>(opPhase, egPhase);
    score += calculateMobilityScore<ROOK  >(opPhase, egPhase);
    score += calculateMobilityScore<QUEEN >(opPhase, egPhase);
    return score;
}

template<PieceType pt>
int Board::calculateMobilityScore(const int opPhase, const int egPhase) const
{
    int mobilityScore = 0;
    BBz occ = occupancy();

    int whitemoves = MoveGen::mobility<pt>(getPieces<pt>(WHITE),
                                          ~getPieces<ALL>(WHITE), occ);
    mobilityScore += whitemoves * (opPhase * Eval::MobilityScaling[OPENING][pt-1]
                                 + egPhase * Eval::MobilityScaling[ENDGAME][pt-1]);
    
    int blackmoves = MoveGen::mobility<pt>(getPieces<pt>(BLACK),
                                          ~getPieces<ALL>(BLACK), occ);
    mobilityScore -= blackmoves * (opPhase * Eval::MobilityScaling[OPENING][pt-1]
                                 + egPhase * Eval::MobilityScaling[ENDGAME][pt-1]);
    
    return mobilityScore;
}

int Board::calculatePieceScore() const
{
    int score = 0;
    score += calculatePieceScore<KNIGHT, WHITE>();
    score -= calculatePieceScore<KNIGHT, BLACK>();
    score += calculatePieceScore<BISHOP, WHITE>();
    score -= calculatePieceScore<BISHOP, BLACK>();
    score += calculatePieceScore<ROOK,   WHITE>();
    score -= calculatePieceScore<ROOK,   BLACK>();
    score += calculatePieceScore<QUEEN,  WHITE>();
    score -= calculatePieceScore<QUEEN,  BLACK>();
    return score;
}

template<PieceType pt, Color c>
int Board::calculatePieceScore() const
{
    int score = 0;

    return score;
}

std::string Board::toFEN() const
{
    std::ostringstream oss;

    for (Rank rank = RANK8; rank >= RANK1; rank--)
    {
        int emptyCount = 0;
        for (File file = FILE1; file <= FILE8; file++) {

            Piece p = getPiece(Types::getSquare(file, rank));
            if (p != EMPTY) {

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

std::string Board::DebugString() const
{
    std::ostringstream oss;
    oss << this;
    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const Board& b)
{
    for (Rank rank = RANK8; rank >= RANK1; rank--)
    {
        os << "   +---+---+---+---+---+---+---+---+\n";
        os << "   |";
        for (File file = FILE1; file <= FILE8; file++)
        {
            Piece p = b.getPiece(Types::getSquare(file, rank));
            os << " " << p << " |";
        }
        os << " " << rank << '\n';
    }

    os << "   +---+---+---+---+---+---+---+---+\n";
    os << "     a   b   c   d   e   f   g   h\n\n";
    os << b.toFEN() << std::endl;
    os << b.state[b.ply].zkey << std::endl;

    return os;
}

void Board::loadFEN(const std::string& fen)
{
    // Split FEN into tokens
    std::vector<std::string> tokens = G::split(fen, ' ');

    if (tokens.size() > 3) {
        loadFENPieces(tokens.at(0));

        if (tokens.at(1)[0] == 'w')
            stm = WHITE;
        else
        {
            stm = BLACK;
            state[ply].zkey ^= Zobrist::stm;
        }

        loadFENCastle(tokens.at(2));
        loadFENEnPassant(tokens.at(3));
    }

    if (tokens.size() > 4)
        state.at(ply).hmClock = std::stoi(tokens.at(4));

    if (tokens.size() > 5)
        fullMoveCounter = 2 * std::stoul(tokens.at(5));
}

void Board::loadFENPieces(const std::string& pieces)
{
    auto file = FILE1;
    auto rank = RANK8;

    for (auto iter = pieces.begin(); iter != pieces.end(); ++iter) {
        auto ch = *iter;

        if (((int) ch > 48) && ((int) ch < 57)) {
            file += File((int)ch - '0');
        } else {
            Square sq = Types::getSquare(file, rank);

            switch (ch) {
                case 'P':
                    addPiece<true>(sq, WHITE, PAWN);
                    file++;
                    break;
                case 'N':
                    addPiece<true>(sq, WHITE, KNIGHT);
                    file++;
                    break;
                case 'B':
                    addPiece<true>(sq, WHITE, BISHOP);
                    file++;
                    break;
                case 'R':
                    addPiece<true>(sq, WHITE, ROOK);
                    file++;
                    break;
                case 'Q':
                    addPiece<true>(sq, WHITE, QUEEN);
                    file++;
                    break;
                case 'K':
                    kingSq[WHITE] = sq;
                    addPiece<true>(sq, WHITE, KING);
                    file++;
                    break;
                case 'p':
                    addPiece<true>(sq, BLACK, PAWN);
                    file++;
                    break;
                case 'n':
                    addPiece<true>(sq, BLACK, KNIGHT);
                    file++;
                    break;
                case 'b':
                    addPiece<true>(sq, BLACK, BISHOP);
                    file++;
                    break;
                case 'r':
                    addPiece<true>(sq, BLACK, ROOK);
                    file++;
                    break;
                case 'q':
                    addPiece<true>(sq, BLACK, QUEEN);
                    file++;
                    break;
                case 'k':
                    kingSq[BLACK] = sq;
                    addPiece<true>(sq, BLACK, KING);
                    file++;
                    break;
                case '/':
                    rank--;
                    file = FILE1;
                    break;
            }
        }
    }
}

void Board::loadFENEnPassant(const std::string& square)
{
    if (square.compare("-") != 0)
        setEnPassant(Types::getSquareFromStr(square));
}

void Board::loadFENCastle(const std::string& castle)
{
    state.at(ply).castle = NO_CASTLE;

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
}

U64 Board::calculateKey() const
{
    U64 zkey = 0x0;

    for (auto sq = A1; sq != INVALID; sq++)
    {
        auto piece = getPiece(sq);

        if (piece != EMPTY)
        {
            auto c = Types::getPieceColor(piece);
            auto pt = Types::getPieceType(piece);
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