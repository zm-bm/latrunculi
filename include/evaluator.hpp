#ifndef LATRUNCULI_EVALUATOR_H
#define LATRUNCULI_EVALUATOR_H

#include <string>

#include "bb.hpp"
#include "board.hpp"
#include "chess.hpp"
#include "eval.hpp"
#include "score.hpp"

enum EvalTerms {
    TERM_MATERIAL,
    TERM_PIECE_SQ,
    TERM_PAWNS,
    N_EVAL_TERMS,
};

template <bool debug = false>
class Evaluator {
   public:
    Evaluator() = delete;
    Evaluator(const Chess& c) : chess{c}, board{c.board} {}

    int eval() const;

   private:
    const Chess& chess;
    const Board& board;
    Score scores[N_EVAL_TERMS][N_COLORS];

    // private methods
    Score pawnsEval() const;
    Score piecesEval() const;

    template <Color>
    Score knightEval() const;
    template <Color>
    Score bishopEval() const;
    template <Color>
    Score queenEval() const;

    int scaleFactor() const;

    int isolatedPawnsCount() const;
    int backwardsPawnsCount() const;
    int doubledPawnsCount() const;

    int knightOutpostCount() const;
    int bishopOutpostCount() const;
    int minorsBehindPawns() const;
    int bishopPawnBlockers() const;

    template <Color c>
    U64 outpostSquares() const;
    bool hasOppositeBishops() const;
    int phase() const;
    int nonPawnMaterial(Color) const;

    friend class EvaluatorTest;
};

template <bool debug>
inline Score Evaluator<debug>::pawnsEval() const {
    Score score = {0, 0};
    score += ISO_PAWN_PENALTY * isolatedPawnsCount();
    score += BACKWARD_PAWN_PENALTY * backwardsPawnsCount();
    score += DOUBLED_PAWN_PENALTY * doubledPawnsCount();
    return score;
}

template <bool debug>
inline Score Evaluator<debug>::piecesEval() const {
    Score score = {0, 0};
    score += KNIGHT_OUTPOST_BONUS * knightOutpostCount();
    score += knightEval<WHITE>() - knightEval<BLACK>();
    score += BISHOP_OUTPOST_BONUS * bishopOutpostCount();
    score += MINOR_BEHIND_PAWN_BONUS * minorsBehindPawns();
    score += BISHOP_PAWN_BLOCKER_PENALTY * bishopPawnBlockers();
    score += bishopEval<WHITE>() - bishopEval<BLACK>();
    score += queenEval<WHITE>() - queenEval<BLACK>();

    return score;
}

template <Color c, typename Func>
void forEachPiece(U64 bitboard, Func action) {
    while (bitboard) {
        Square sq = BB::advanced<WHITE>(bitboard);
        action(sq);
        bitboard &= BB::clear(sq);
    }
}

template <bool debug>
template <Color c>
inline Score Evaluator<debug>::knightEval() const {
    Score score = {0, 0};
    U64 outposts = outpostSquares<c>();

    forEachPiece<c>(board.getPieces<KNIGHT>(c), [&](Square sq) {
        U64 reachableOutposts = BB::movesByPiece<KNIGHT>(sq, 0) & outposts;
        score += REACHABLE_OUTPOST_BONUS * BB::bitCount(reachableOutposts);
    });

    return score;
}

template <bool debug>
template <Color c>
inline Score Evaluator<debug>::bishopEval() const {
    constexpr Color enemy = ~c;
    Score score = {0, 0};
    U64 enemyPawns = board.getPieces<PAWN>(enemy);

    forEachPiece<c>(board.getPieces<BISHOP>(c), [&](Square sq) {
        U64 moves = BB::movesByPiece<BISHOP>(sq, 0);
        score += BISHOP_PAWN_XRAY_PENALTY * BB::bitCount(moves & enemyPawns);
    });

    return score;
}

template <bool debug>
template <Color c>
inline Score Evaluator<debug>::queenEval() const {
    Score score = {0, 0};
    U64 rooksOnQueenFile = 0;

    forEachPiece<c>(board.getPieces<QUEEN>(c), [&](Square sq) {
        File f = Defs::fileFromSq(sq);
        U64 mask = BB::filemask(f, WHITE);
        rooksOnQueenFile |= mask & board.getPieces<ROOK>(c);
    });

    score += ROOK_ON_QUEEN_FILE_BONUS * BB::bitCount(rooksOnQueenFile);

    return score;
}

template <bool debug>
inline int Evaluator<debug>::isolatedPawnsCount() const {
    U64 wIsolatedPawns = Eval::isolatedPawns(board.getPieces<PAWN>(WHITE));
    U64 bIsolatedPawns = Eval::isolatedPawns(board.getPieces<PAWN>(BLACK));
    return BB::bitCount(wIsolatedPawns) - BB::bitCount(bIsolatedPawns);
}

template <bool debug>
inline int Evaluator<debug>::backwardsPawnsCount() const {
    U64 wPawns = board.getPieces<PAWN>(WHITE);
    U64 bPawns = board.getPieces<PAWN>(BLACK);
    U64 wBackwardsPawns = Eval::backwardsPawns<WHITE>(wPawns, bPawns);
    U64 bBackwardsPawns = Eval::backwardsPawns<BLACK>(bPawns, wPawns);
    return BB::bitCount(wBackwardsPawns) - BB::bitCount(bBackwardsPawns);
}

template <bool debug>
inline int Evaluator<debug>::doubledPawnsCount() const {
    U64 wDoubledPawns = Eval::doubledPawns<WHITE>(board.getPieces<PAWN>(WHITE));
    U64 bDoubledPawns = Eval::doubledPawns<BLACK>(board.getPieces<PAWN>(BLACK));
    return BB::bitCount(wDoubledPawns) - BB::bitCount(bDoubledPawns);
}

template <bool debug>
inline int Evaluator<debug>::knightOutpostCount() const {
    U64 wOutposts = outpostSquares<WHITE>();
    U64 bOutposts = outpostSquares<BLACK>();
    return (BB::bitCount(board.getPieces<KNIGHT>(WHITE) & wOutposts) -
            BB::bitCount(board.getPieces<KNIGHT>(BLACK) & bOutposts));
}

template <bool debug>
inline int Evaluator<debug>::bishopOutpostCount() const {
    U64 wOutposts = outpostSquares<WHITE>();
    U64 bOutposts = outpostSquares<BLACK>();
    return (BB::bitCount(board.getPieces<BISHOP>(WHITE) & wOutposts) -
            BB::bitCount(board.getPieces<BISHOP>(BLACK) & bOutposts));
}

template <bool debug>
inline int Evaluator<debug>::minorsBehindPawns() const {
    U64 wPawnsInFront = BB::movesByPawns<PawnMove::PUSH, BLACK>(board.getPieces<PAWN>(WHITE));
    U64 bPawnsInFront = BB::movesByPawns<PawnMove::PUSH, WHITE>(board.getPieces<PAWN>(BLACK));
    U64 wMinors = board.getPieces<KNIGHT>(WHITE) | board.getPieces<BISHOP>(WHITE);
    U64 bMinors = board.getPieces<KNIGHT>(BLACK) | board.getPieces<BISHOP>(BLACK);
    U64 wMinorsBehind = wMinors & wPawnsInFront;
    U64 bMinorsBehind = bMinors & bPawnsInFront;
    return BB::bitCount(wMinorsBehind) - BB::bitCount(bMinorsBehind);
}

template <bool debug>
inline int Evaluator<debug>::bishopPawnBlockers() const {
    U64 wPawns = board.getPieces<PAWN>(WHITE);
    U64 bPawns = board.getPieces<PAWN>(BLACK);
    return (Eval::bishopPawnBlockers<WHITE>(board.getPieces<BISHOP>(WHITE), wPawns, bPawns) -
            Eval::bishopPawnBlockers<BLACK>(board.getPieces<BISHOP>(BLACK), bPawns, wPawns));
}

template <bool debug>
template <Color c>
inline U64 Evaluator<debug>::outpostSquares() const {
    constexpr Color enemy = ~c;
    return Eval::outpostSquares<c>(board.getPieces<PAWN>(c), board.getPieces<PAWN>(enemy));
}

template <bool debug>
inline bool Evaluator<debug>::hasOppositeBishops() const {
    if (board.count<BISHOP>(WHITE) != 1 || board.count<BISHOP>(BLACK) != 1) {
        return false;
    }
    U64 wBishops = board.getPieces<BISHOP>(WHITE);
    U64 bBishops = board.getPieces<BISHOP>(BLACK);
    return (((wBishops & LIGHT_SQUARES) && (bBishops & DARK_SQUARES)) ||
            ((wBishops & DARK_SQUARES) && (bBishops & LIGHT_SQUARES)));
}

template <bool debug>
inline int Evaluator<debug>::phase() const {
    int totalNpm = nonPawnMaterial(WHITE) + nonPawnMaterial(BLACK);
    int material = std::clamp(totalNpm, EG_LIMIT, MG_LIMIT);
    return ((material - EG_LIMIT) * PHASE_LIMIT) / (MG_LIMIT - EG_LIMIT);
}

template <bool debug>
inline int Evaluator<debug>::nonPawnMaterial(Color c) const {
    return (board.count<KNIGHT>(c) * Eval::pieceValue(MIDGAME, WHITE, KNIGHT) +
            board.count<BISHOP>(c) * Eval::pieceValue(MIDGAME, WHITE, BISHOP) +
            board.count<ROOK>(c) * Eval::pieceValue(MIDGAME, WHITE, ROOK) +
            board.count<QUEEN>(c) * Eval::pieceValue(MIDGAME, WHITE, QUEEN));
}

template <bool debug>
int Evaluator<debug>::eval() const {
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

template <bool debug>
int Evaluator<debug>::scaleFactor() const {
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



#endif
