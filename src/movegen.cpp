#include "movegen.hpp"

#include "chess.hpp"

MoveGenerator::MoveGenerator(Chess* chess) : chess(chess) {}

void MoveGenerator::generatePseudoLegalMoves() {
    if (chess->isCheck()) {
        generateEvasions();
    } else {
        Color enemy = ~chess->turn;
        U64 targets = chess->board.getPieces<ALL_PIECE_TYPES>(enemy);
        generateMovesToTarget<CAPTURES>(targets);

        targets = ~chess->board.occupancy();
        generateMovesToTarget<QUIETS>(targets);
    }
}

void MoveGenerator::generateCaptures() {
    if (chess->isCheck()) {
        generateEvasions();
    } else {
        Color enemy = ~chess->turn;
        U64 targets = chess->board.getPieces<ALL_PIECE_TYPES>(enemy);
        generateMovesToTarget<CAPTURES>(targets);
    }
}

void MoveGenerator::generateEvasions() {
    Color turn = chess->turn;

    // If in double check, try to capture the checking piece or block
    U64 targets;
    if (!chess->isDoubleCheck()) {
        // Get square of the checking piece and checked king
        Square checker = BB::lsb(chess->getCheckingPieces());
        Square king = chess->board.getKingSq(turn);

        // Generate moves which block or capture the checking piece
        // NOTE: generateMovesToTarget<EVASIONS> does NOT generate king moves
        targets = BB::bitsBtwn(checker, king) | BB::set(checker);
        generateMovesToTarget<EVASIONS>(targets);
    }

    // Generate king evasions
    targets = ~chess->board.getPieces<ALL_PIECE_TYPES>(turn);
    generateKingMoves<EVASIONS>(targets, chess->board.occupancy());
}

template <GenType g>
void MoveGenerator::generateMovesToTarget(const U64 targets) {
    U64 occ = chess->board.occupancy();

    if (chess->turn == WHITE) {
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

template <GenType g, Color c>
void MoveGenerator::generatePawnMoves(const U64 targets, const U64 occ) {
    Color enemy = ~chess->turn;
    U64 bitboard, enemies, vacancies = ~occ;

    // Determine which enemies to target depending on the type of move
    // generation
    if (g == EVASIONS) {
        // If generating evasions, only attack checking pieces
        enemies = chess->board.getPieces<ALL_PIECE_TYPES>(enemy) & targets;
    } else if (g == CAPTURES) {
        // If generating captures, targets are all enemy pieces
        enemies = targets;
    } else if (g == QUIETS) {
        // If generating quiet moves, get enemy pieces
        enemies = chess->board.getPieces<ALL_PIECE_TYPES>(enemy);
    }

    // Get 7th rank pawns
    U64 pawns = chess->board.getPieces<PAWN>(c) & BB::rankmask(RANK7, c);

    // Generate pawn promotions, if there are targets attacked by 7th rank pawns
    if (pawns && (g != EVASIONS || (targets & BB::rankmask(RANK8, c)))) {
        // push / regular move promotions
        bitboard = (g == EVASIONS)
            ? BB::movesByPawns<PawnMove::PUSH, c>(pawns) & vacancies & targets
            : BB::movesByPawns<PawnMove::PUSH, c>(pawns) & vacancies;
        addPawnPromotions<PawnMove::PUSH, c, g>(bitboard);

        // capture promotions
        bitboard = BB::movesByPawns<PawnMove::LEFT, c>(pawns) & enemies;
        addPawnPromotions<PawnMove::LEFT, c, g>(bitboard);
        bitboard = BB::movesByPawns<PawnMove::RIGHT, c>(pawns) & enemies;
        addPawnPromotions<PawnMove::RIGHT, c, g>(bitboard);
    }

    // Get non 7th rank pawns
    pawns = chess->board.getPieces<PAWN>(c) & ~BB::rankmask(RANK7, c);

    if (g != QUIETS) {
        // Generate captures
        bitboard = BB::movesByPawns<PawnMove::LEFT, c>(pawns) & enemies;
        addPawnMoves<PawnMove::LEFT, c>(bitboard);
        bitboard = BB::movesByPawns<PawnMove::RIGHT, c>(pawns) & enemies;
        addPawnMoves<PawnMove::RIGHT, c>(bitboard);

        // Generate en passants
        Square enpassant = chess->getEnPassant();
        if (enpassant != INVALID) {
            // Only necessary if enemy pawn is targeted, or if in check
            Square enemyPawn =
                Defs::pawnMove<c, PawnMove::PUSH, false>(enpassant);
            if (g != EVASIONS || (targets & BB::set(enemyPawn))) {
                // Append en passant captures to move list
                addEnPassants<PawnMove::LEFT, c>(pawns, enpassant);
                addEnPassants<PawnMove::RIGHT, c>(pawns, enpassant);
            }
        }
    }

    // Generate regular pawn pushes
    if (g != CAPTURES) {
        bitboard = BB::movesByPawns<PawnMove::PUSH, c>(pawns) & vacancies;
        U64 doubleMovePawns = bitboard & BB::rankmask(RANK3, c),
            doubleMoves = BB::movesByPawns<PawnMove::PUSH, c>(doubleMovePawns) &
                          vacancies;

        // If in check, only make moves that block
        if (g == EVASIONS) {
            doubleMoves &= targets;
            bitboard &= targets;
        }

        addPawnMoves<PawnMove::DOUBLE, c>(doubleMoves);
        addPawnMoves<PawnMove::PUSH, c>(bitboard);
    }
}

template <GenType g>
void MoveGenerator::generateKingMoves(const U64 targets, const U64 occ) {
    Color turn = chess->turn;
    Square from = chess->board.getKingSq(turn);

    U64 kingMoves = BB::movesByPiece<KING>(from) & targets;

    while (kingMoves) {
        Square to = BB::lsb(kingMoves);
        kingMoves &= BB::clear(to);
        moves.push_back(Move(from, to));
    }

    if (g == QUIETS && chess->state.at(chess->ply).canCastle(turn)) {
        Square from = KingOrigin[turn];

        if (canCastleOO(occ, turn)) {
            Move mv = Move(from, KingDestinationOO[turn], CASTLE);
            moves.push_back(mv);
        }

        if (canCastleOOO(occ, turn)) {
            Move mv = Move(from, KingDestinationOOO[turn], CASTLE);
            moves.push_back(mv);
        }
    }
}

template <PieceType p, Color c>
void MoveGenerator::generatePieceMoves(const U64 targets, const U64 occ) {
    Color turn = chess->turn;
    U64 bitboard = chess->board.getPieces<p>(turn);

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
