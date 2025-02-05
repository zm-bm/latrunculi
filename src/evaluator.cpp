#include "evaluator.hpp"

template <bool debug = false>
int Evaluator::eval() const {
    Score score{0, 0};

    score += chess.materialScore();
    score += chess.psqBonusScore();
    score += pawnsEval();
    score += piecesEval();

    score.eg *= scaleFactor() / 64.0;

    // tapered eval based on remaining non pawn material
    int result = score.taper(phase());

    // return score relative to side to move with tempo bonus
    if (chess.turn == WHITE) {
        result = result + TEMPO_BONUS;
    } else {
        result = -result - TEMPO_BONUS;
    }

    if constexpr (debug) {
        std::cout << "score: " << result << std::endl;
    }

    return result;
}
template int Evaluator::eval<true>() const;
template int Evaluator::eval<false>() const;

int Evaluator::scaleFactor() const {
    Color enemy = ~chess.turn;
    int pawnCount = board.count<PAWN>(chess.turn);
    int pawnCountEnemy = board.count<PAWN>(enemy);
    int nonPawnMat = nonPawnMaterial(chess.turn);
    int nonPawnMatEnemy = nonPawnMaterial(enemy);
    int nonPawnMatDiff = std::abs(nonPawnMat - nonPawnMatEnemy);

    // Check for drawish scenarios with no pawns and equal material
    if (pawnCount == 0 && pawnCountEnemy == 0 &&
        nonPawnMatDiff <= Eval::pieceValue(MIDGAME, WHITE, BISHOP)) {
        return nonPawnMat < Eval::pieceValue(MIDGAME, WHITE, ROOK) ? 0 : 16;
    }

    // Opposite-colored bishops often lead to draws
    if (hasOppositeBishops()) {
        // todo: use candidate passed pawns
        U64 wPawns = board.getPieces<PAWN>(WHITE);
        U64 bPawns = board.getPieces<PAWN>(BLACK);
        U64 passedPawns = (chess.turn == WHITE) ? Eval::passedPawns<WHITE>(wPawns, bPawns)
                                                : Eval::passedPawns<WHITE>(bPawns, wPawns);
        return std::min(SCALE_LIMIT, 36 + 4 * BB::bitCount(passedPawns));
    }

    // Single queen scenarios with minor pieces
    int queenCount = board.count<QUEEN>(chess.turn);
    if (queenCount + board.count<QUEEN>(enemy) == 1) {
        Color side = queenCount == 1 ? enemy : chess.turn;
        int minorPieceCount = board.count<BISHOP>(side) + board.count<KNIGHT>(side);
        return std::min(SCALE_LIMIT, 36 + 4 * minorPieceCount);
    }

    // Default: scale proportionally with pawns
    return std::min(SCALE_LIMIT, 36 + 5 * pawnCount);
}

