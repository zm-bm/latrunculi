#include "movegen.hpp"
#include "chess.hpp"

namespace MoveGen
{

    void Generator::run() {
        moves.clear();

        if (chess->isCheck()) {
            generateEvasions();
        } else {
            Color enemy = ~chess->stm;
            U64 targets = chess->board.getPieces<ALL_PIECE_ROLES>(enemy);
            generate<CAPTURES>(targets);

            targets = ~chess->board.occupancy();
            generate<QUIETS>(targets);
        }
    }

    void Generator::runq() {
        moves.clear();

        if (chess->isCheck()) {
            generateEvasions();
        } else {
            Color enemy = ~chess->stm;
            U64 targets = chess->board.getPieces<ALL_PIECE_ROLES>(enemy);
            generate<CAPTURES>(targets);
        }
    }


    template<MoveGenType g>
    void Generator::generate(const U64 targets) {
        U64 occ = chess->board.occupancy();
        
        if (chess->stm == WHITE) {
            generatePieceMoves<KNIGHT, WHITE>(targets, occ);
            generatePieceMoves<BISHOP, WHITE>(targets, occ);
            generatePieceMoves<ROOK, WHITE>(targets, occ);
            generatePieceMoves<QUEEN, WHITE>(targets, occ);
            generatePawnMoves<g, WHITE>(targets, occ);
        } else {
            generatePieceMoves<KNIGHT, BLACK>(targets, occ);
            generatePieceMoves<BISHOP, BLACK>(targets, occ);
            generatePieceMoves<ROOK, BLACK>(targets, occ);
            generatePieceMoves<QUEEN, BLACK>(targets, occ);
            generatePawnMoves<g, BLACK>(targets, occ);
        }

        // Skip king moves if in check, generated separately in generateEvasions
        if (g != EVASIONS) {
            generateKingMoves<g>(targets, occ);
        }
    }

    template<MoveGenType g, Color c>
    void Generator::generatePawnMoves(const U64 targets, const U64 occ) {
        // Determine which enemies to target depending on the type of move generation
        Color enemy = ~chess->stm;
        U64 enemies, vacancies = ~occ;
        if (g == EVASIONS) {
            // If generating evasions, only attack checking pieces
            enemies = chess->board.getPieces<ALL_PIECE_ROLES>(enemy) & targets;
        } else if (g == CAPTURES) {
            // If generating captures, targets are all enemy pieces
            enemies = targets;
        } else if (g == QUIETS) {
            // If generating quiet moves, get enemy pieces
            enemies = chess->board.getPieces<ALL_PIECE_ROLES>(enemy);
        }

        // Get 7th rank pawns
        U64 pawns = chess->board.getPieces<PAWN>(c) & G::rankmask(RANK7, c);

        // Generate pawn promotions, if there are targets attacked by 7th rank pawns
        U64 bitboard;
        if (pawns && (g != EVASIONS || (targets & G::rankmask(RANK8, c)))) {
            // Generate legal pawn push bitboard
            // If in check, only promote if the move blocks
            if (g == EVASIONS) {
                bitboard = BB::movesByPawns<PawnMove::PUSH, c>(pawns) & vacancies & targets;
            } else {
                bitboard = BB::movesByPawns<PawnMove::PUSH, c>(pawns) & vacancies;
            }
            appendPawnPromotions<PawnMove::PUSH, c, g>(bitboard);
            
            // Append capturing promotions to move list
            bitboard = BB::movesByPawns<PawnMove::LEFT, c>(pawns) & enemies;
            appendPawnPromotions<PawnMove::LEFT, c, g>(bitboard);

            bitboard = BB::movesByPawns<PawnMove::RIGHT, c>(pawns) & enemies;
            appendPawnPromotions<PawnMove::RIGHT, c, g>(bitboard);
        }

        // Get non 7th rank pawns
        pawns = chess->board.getPieces<PAWN>(c) & ~G::rankmask(RANK7, c);

        // Generate captures
        if (g != QUIETS) {
            // Append capturing moves to move list
            bitboard = BB::movesByPawns<PawnMove::LEFT, c>(pawns) & enemies;
            appendPawnMoves<PawnMove::LEFT, c>(bitboard);
            bitboard = BB::movesByPawns<PawnMove::RIGHT, c>(pawns) & enemies;
            appendPawnMoves<PawnMove::RIGHT, c>(bitboard);

            // Generate en passants
            Square enpassant = chess->getEnPassant();
            if (enpassant != INVALID) {
                // Only necessary if enemy pawn is targeted, or if in check
                Square enemyPawn = Types::pawnMove<c, PawnMove::PUSH, false>(enpassant);
                if (g != EVASIONS || (targets & BB::set(enemyPawn))) {
                    // Append en passant captures to move list
                    bitboard = BB::movesByPawns<PawnMove::LEFT, c>(pawns) & BB::set(enpassant);
                    if (bitboard) {
                        Square from = Types::pawnMove<c, PawnMove::LEFT, false>(enpassant);
                        moves.push_back(Move(from, enpassant, ENPASSANT));
                    }
                    bitboard = BB::movesByPawns<PawnMove::RIGHT, c>(pawns) & BB::set(enpassant);
                    if (bitboard) {
                        Square from = Types::pawnMove<c, PawnMove::RIGHT, false>(enpassant);
                        moves.push_back(Move(from, enpassant, ENPASSANT));
                    }
                }
            }
        }

        // Generate regular pawn pushes
        if (g != CAPTURES) {
            bitboard = BB::movesByPawns<PawnMove::PUSH, c>(pawns) & vacancies;
            U64 doubleMovePawns = bitboard & G::rankmask(RANK3, c),
               doubleMoves = BB::movesByPawns<PawnMove::PUSH, c>(doubleMovePawns) & vacancies;

            // If in check, only make moves that block
            if (g == EVASIONS) {
                doubleMoves &= targets;
                bitboard &= targets;
            }

            appendPawnMoves<PawnMove::DOUBLE, c>(doubleMoves);
            appendPawnMoves<PawnMove::PUSH, c>(bitboard);
        }
    }

    template<MoveGenType g>
    void Generator::generateKingMoves(const U64 targets, const U64 occ) {
        Color stm = chess->stm;
        Square from = chess->board.getKingSq(stm);

        U64 kingMoves = BB::movesByPiece<KING>(from, occ) & targets;

        while (kingMoves) {
            Square to = BB::lsb(kingMoves);
            kingMoves &= BB::clear(to);
            moves.push_back(Move(from, to));
        }

        if (g == QUIETS && chess->canCastle(stm)) {
            generateCastling();
        }
    }

    template<PieceRole p, Color c>
    void Generator::generatePieceMoves(const U64 targets, const U64 occ) {
        Color stm = chess->stm;
        U64 bitboard = chess->board.getPieces<p>(stm);

        while (bitboard) {
            // Pop lsb bit and clear it from the bitboard
            Square from = BB::advanced<c>(bitboard);
            bitboard &= BB::clear(from);

            U64 pieceMoves = BB::movesByPiece<p>(from, occ) & targets;
            while (pieceMoves) {
                Square to = BB::advanced<c>(pieceMoves);
                pieceMoves &= BB::clear(to);

                moves.push_back(Move(from, to));
            }
        }
    }

    void Generator::generateEvasions() {
        Color stm = chess->stm;

        // If in double check, try to capture the checking piece or block
        U64 targets;
        if (!chess->isDoubleCheck()) {
            // Get square of the checking piece and checked king
            Square checker = BB::lsb(chess->getCheckingPieces());
            Square king = chess->board.getKingSq(stm);

            // Generate moves which block or capture the checking piece
            // NOTE: generate<EVASIONS> does NOT generate king moves
            targets = BB::bitsBtwn(checker, king) | BB::set(checker);
            generate<EVASIONS>(targets);
        }

        // Generate king evasions
        targets = ~chess->board.getPieces<ALL_PIECE_ROLES>(stm);
        generateKingMoves<EVASIONS>(targets, chess->board.occupancy());
    }

    void Generator::generateCastling()
    {
        Color stm = chess->stm;
        U64 occ = chess->board.occupancy();
        Square from = G::KingOrigin[stm];

        if (
            chess->canCastleOO(stm) &&
            !(occ & G::CastlePathOO[stm]) &&
            !chess->board.isBitboardAttacked(G::KingCastlePathOO[stm], ~stm)
        ) {
            Square to = G::KingDestinationOO[stm];
            Move mv = Move(from, to, CASTLE);
            moves.push_back(mv);
        }

        if (
            chess->canCastleOOO(stm) &&
            !(occ & G::CastlePathOOO[stm]) &&
            !chess->board.isBitboardAttacked(G::KingCastlePathOOO[stm], ~stm)
        ) {
            Square to = G::KingDestinationOOO[stm];
            Move mv = Move(from, to, CASTLE);
            moves.push_back(mv);
        }
    }

    template<PawnMove p, Color c>
    void Generator::appendPawnMoves(U64 bitboard)
    {
        while (bitboard)
        {
            // Pop lsb bit and clear it from the bitboard
            Square to = BB::advanced<c>(bitboard);
            bitboard &= BB::clear(to);

            // Reverse the move to get the origin square and append move
            Square from = Types::pawnMove<c, p, false>(to);
            moves.push_back(Move(from, to));
        }
    };

    template<PawnMove p, Color c, MoveGenType g>
    void Generator::appendPawnPromotions(U64 bitboard)
    {
        while (bitboard)
        {
            // Pop lsb bit and clear it from the bitboard
            Square to = BB::advanced<c>(bitboard);
            bitboard &= BB::clear(to);

            // Reverse the move to get the origin square and append move(s)
            Square from = Types::pawnMove<c, p, false>(to);

            if (g != QUIETS)
                moves.push_back(Move(from, to, PROMOTION, QUEEN));
            if (g != CAPTURES)
            {
                moves.push_back(Move(from, to, PROMOTION, ROOK));
                moves.push_back(Move(from, to, PROMOTION, BISHOP));
                moves.push_back(Move(from, to, PROMOTION, KNIGHT));
            }
        }
    }

}

