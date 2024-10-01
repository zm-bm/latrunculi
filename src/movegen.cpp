#include "movegen.hpp"
#include "board.hpp"

namespace MoveGen
{

    void Generator::run()
    {
        moves.clear();

        if (b->isCheck())
            generateEvasions();
        else
        {
            Color enemy = ~b->sideToMove();
            U64 targets = b->getPieces<ALL_PIECE_ROLES>(enemy);
            generate<CAPTURES>(targets);

            targets = ~b->occupancy();
            generate<QUIETS>(targets);
        }
    }

    void Generator::runq()
    {
        moves.clear();

        if (b->isCheck())
            generateEvasions();
        else
        {
            Color enemy = ~b->sideToMove();
            U64 targets = b->getPieces<ALL_PIECE_ROLES>(enemy);
            generate<CAPTURES>(targets);
        }
    }


    template<MoveGenType g>
    void Generator::generate(const U64 targets)
    {
        U64 occ = b->occupancy();
        
        if (b->sideToMove() == WHITE)
        {
            generatePieceMoves<KNIGHT, WHITE>(targets, occ);
            generatePieceMoves<BISHOP, WHITE>(targets, occ);
            generatePieceMoves<ROOK, WHITE>(targets, occ);
            generatePieceMoves<QUEEN, WHITE>(targets, occ);
            generatePawnMoves<g, WHITE>(targets, occ);
        }
        else
        {
            generatePieceMoves<KNIGHT, BLACK>(targets, occ);
            generatePieceMoves<BISHOP, BLACK>(targets, occ);
            generatePieceMoves<ROOK, BLACK>(targets, occ);
            generatePieceMoves<QUEEN, BLACK>(targets, occ);
            generatePawnMoves<g, BLACK>(targets, occ);
        }

        // Skip king moves if in check, generated separately in generateEvasions
        if (g != EVASIONS)
            generateKingMoves<g>(targets, occ);
    }

    template<MoveGenType g, Color c>
    void Generator::generatePawnMoves(const U64 targets, const U64 occ)
    {
        // Determine which enemies to target depending on the type of move generation
        Color enemy = ~b->sideToMove();
        U64 enemies, vacancies = ~occ;
        if (g == EVASIONS)
            // If generating evasions, only attack checking pieces
            enemies = b->getPieces<ALL_PIECE_ROLES>(enemy) & targets;
        else if (g == CAPTURES)
            // If generating captures, targets are all enemy pieces
            enemies = targets;
        else if (g == QUIETS)
            // If generating quiet moves, get enemy pieces
            enemies = b->getPieces<ALL_PIECE_ROLES>(enemy);

        // Get 7th rank pawns
        U64 pawns = b->getPieces<PAWN>(c) & G::rankmask(RANK7, c);

        // Generate pawn promotions, if there are targets attacked by 7th rank pawns
        U64 bitboard;
        if (pawns && (g != EVASIONS || (targets & G::rankmask(RANK8, c))))
        {
            // Generate legal pawn push bitboard
            // If in check, only promote if the move blocks
            if (g == EVASIONS)
                bitboard = MoveGen::movesByPawns<PawnMove::PUSH, c>(pawns) & vacancies & targets;
            else
                bitboard = MoveGen::movesByPawns<PawnMove::PUSH, c>(pawns) & vacancies;
            appendPawnPromotions<PawnMove::PUSH, c, g>(bitboard);
            
            // Append capturing promotions to move list
            bitboard = MoveGen::movesByPawns<PawnMove::LEFT, c>(pawns) & enemies;
            appendPawnPromotions<PawnMove::LEFT, c, g>(bitboard);

            bitboard = MoveGen::movesByPawns<PawnMove::RIGHT, c>(pawns) & enemies;
            appendPawnPromotions<PawnMove::RIGHT, c, g>(bitboard);
        }

        // Get non 7th rank pawns
        pawns = b->getPieces<PAWN>(c) & ~G::rankmask(RANK7, c);

        // Generate captures
        if (g != QUIETS)
        {
            // Append capturing moves to move list
            bitboard = MoveGen::movesByPawns<PawnMove::LEFT, c>(pawns) & enemies;
            appendPawnMoves<PawnMove::LEFT, c>(bitboard);
            bitboard = MoveGen::movesByPawns<PawnMove::RIGHT, c>(pawns) & enemies;
            appendPawnMoves<PawnMove::RIGHT, c>(bitboard);

            // Generate en passants
            Square enpassant = b->getEnPassant();
            if (enpassant != INVALID)
            {
                // Only necessary if enemy pawn is targeted, or if in check
                Square enemyPawn = Types::pawnMove<c, PawnMove::PUSH, false>(enpassant);
                if (g != EVASIONS || (targets & BB::set(enemyPawn)))
                {
                    // Append en passant captures to move list
                    bitboard = MoveGen::movesByPawns<PawnMove::LEFT, c>(pawns) & BB::set(enpassant);
                    if (bitboard) {
                        Square from = Types::pawnMove<c, PawnMove::LEFT, false>(enpassant);
                        moves.push_back(Move(from, enpassant, ENPASSANT));
                    }
                    bitboard = MoveGen::movesByPawns<PawnMove::RIGHT, c>(pawns) & BB::set(enpassant);
                    if (bitboard) {
                        Square from = Types::pawnMove<c, PawnMove::RIGHT, false>(enpassant);
                        moves.push_back(Move(from, enpassant, ENPASSANT));
                    }
                }
            }
        }

        // Generate regular pawn pushes
        if (g != CAPTURES)
        {
            bitboard = MoveGen::movesByPawns<PawnMove::PUSH, c>(pawns) & vacancies;
            U64 doubleMovePawns = bitboard & G::rankmask(RANK3, c),
               doubleMoves = MoveGen::movesByPawns<PawnMove::PUSH, c>(doubleMovePawns) & vacancies;

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
    void Generator::generateKingMoves(const U64 targets, const U64 occ)
    {
        Color stm = b->sideToMove();
        Square from = b->getKingSq(stm);

        U64 kingMoves = movesByPiece<KING>(from, occ) & targets;

        while (kingMoves)
        {
            Square to = BB::lsb(kingMoves);
            kingMoves &= G::BITCLEAR[to];
            moves.push_back(Move(from, to));
        }

        if (g == QUIETS && b->canCastle(stm))
            generateCastling();
    }

    template<PieceRole p, Color c>
    void Generator::generatePieceMoves(const U64 targets, const U64 occ)
    {
        Color stm = b->sideToMove();
        U64 bitboard = b->getPieces<p>(stm);

        while (bitboard)
        {
            // Pop lsb bit and clear it from the bitboard
            Square from = BB::advanced<c>(bitboard);
            bitboard &= G::BITCLEAR[from];

            U64 pieceMoves = movesByPiece<p>(from, occ) & targets;
            while (pieceMoves)
            {
                Square to = BB::advanced<c>(pieceMoves);
                pieceMoves &= G::BITCLEAR[to];

                moves.push_back(Move(from, to));
            }
        }
    }

    void Generator::generateEvasions()
    {
        Color stm = b->sideToMove();

        // If in double check, try to capture the checking piece or block
        U64 targets;
        if (!b->isDoubleCheck())
        {
            // Get square of the checking piece and checked king
            Square checker = BB::lsb(b->getCheckingPieces());
            Square king = b->getKingSq(stm);

            // Generate moves which block or capture the checking piece
            // NOTE: generate<EVASIONS> does NOT generate king moves
            targets = G::BITS_BETWEEN[checker][king] | BB::set(checker);
            generate<EVASIONS>(targets);
        }

        // Generate king evasions
        targets = ~b->getPieces<ALL_PIECE_ROLES>(stm);
        generateKingMoves<EVASIONS>(targets, b->occupancy());
    }

    void Generator::generateCastling()
    {
        Color stm = b->sideToMove();
        U64 occ = b->occupancy();
        Square from = MoveGen::KingOrigin[stm];

        if (b->canCastleOO(stm) &&
        !(occ & MoveGen::CastlePathOO[stm]) &&
        !b->isBitboardAttacked(MoveGen::KingCastlePathOO[stm], ~stm))
        {
            Square to = MoveGen::KingDestinationOO[stm];
            Move mv = Move(from, to, CASTLE);
            moves.push_back(mv);
        }

        if (b->canCastleOOO(stm) &&
        !(occ & MoveGen::CastlePathOOO[stm]) &&
        !b->isBitboardAttacked(MoveGen::KingCastlePathOOO[stm], ~stm))
        {
            Square to = MoveGen::KingDestinationOOO[stm];
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
            bitboard &= G::BITCLEAR[to];

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
            bitboard &= G::BITCLEAR[to];

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

    const U64 CastlePathOO[3]          = { 0x6000000000000000ull, 0x0000000000000060ull };
    const U64 CastlePathOOO[2]         = { 0x0E00000000000000ull, 0x000000000000000Eull };
    const U64 KingCastlePathOO[2]      = { 0x7000000000000000ull, 0x0000000000000070ull };
    const U64 KingCastlePathOOO[2]     = { 0x1C00000000000000ull, 0x000000000000001Cull };

    const Square KingOrigin[2]        = { E8, E1 };
    const Square KingDestinationOO[2] = { G8, G1 };
    const Square KingDestinationOOO[2]= { C8, C1 };
    const Square RookOriginOO[2]      = { H8, H1 };
    const Square RookOriginOOO[2]     = { A8, A1 };

}

