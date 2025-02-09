#ifndef LATRUNCULI_EVALUATOR_H
#define LATRUNCULI_EVALUATOR_H

#include <optional>
#include <string>

#include "bb.hpp"
#include "board.hpp"
#include "chess.hpp"
#include "eval.hpp"
#include "score.hpp"

enum EvalTerm {
    TERM_MATERIAL,
    TERM_PIECE_SQ,
    TERM_PAWNS,
    TERM_PIECES,
    N_EVAL_TERMS,
};

template <bool debug = false>
class Evaluator {
   public:
    Evaluator() = delete;
    Evaluator(const Chess& c) : chess{c}, board{c.board} {}
    int eval();

   private:
    const Chess& chess;
    const Board& board;

    using ScoresDetail =
        typename std::conditional<debug, Score[N_EVAL_TERMS][N_COLORS], std::nullptr_t>::type;
    ScoresDetail scores{};

    template <EvalTerm, Color>
    Score evalTerm();

    template <Color>
    Score pawnsEval() const;

    friend class EvaluatorTest;

    struct EvalTermLine {
        std::string name;
        Score whiteScore;
        std::optional<Score> blackScore;

        friend std::ostream& operator<<(std::ostream& os, const EvalTermLine& line) {
            os << std::setw(12) << line.name << " | ";
            if (line.blackScore) {
                os << line.whiteScore << " | " << *line.blackScore << " | "
                   << line.whiteScore - *line.blackScore << '\n';
            } else {
                os << "----- ----- | ----- ----- | " << line.whiteScore << '\n';
            }
            return os;
        }
    };

    // old code
    Score piecesEval() const;

    template <Color>
    Score knightEval() const;
    template <Color>
    Score bishopEval() const;
    template <Color>
    Score queenEval() const;

    int knightOutpostCount() const;
    int bishopOutpostCount() const;
    int minorsBehindPawns() const;
    int bishopPawnBlockers() const;

    template <Color c>
    U64 outpostSquares() const;
    bool hasOppositeBishops() const;
    int phase() const;
    int nonPawnMaterial(Color) const;

    int scaleFactor() const;
};

template <bool debug>
template <EvalTerm term, Color c>
Score Evaluator<debug>::evalTerm() {
    Score score;

    switch (term) {
        case TERM_MATERIAL: score = chess.materialScore(); break;
        case TERM_PIECE_SQ: score = chess.psqBonusScore(); break;
        case TERM_PAWNS: score = pawnsEval<c>(); break;
        default: score = Score{0}; break;
    }

    if constexpr (debug) {
        scores[term][c] = score;
    }

    return score;
}

template <bool debug>
int Evaluator<debug>::eval() {
    Score score{0, 0};

    score += evalTerm<TERM_MATERIAL, WHITE>();
    score += evalTerm<TERM_PIECE_SQ, WHITE>();
    score += evalTerm<TERM_PAWNS, WHITE>() - evalTerm<TERM_PAWNS, BLACK>();

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
        std::cout << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2)
                  << "     Term    |    White    |    Black    |    Total   \n"
                  << "             |   MG    EG  |   MG    EG  |   MG    EG \n"
                  << " ------------+-------------+-------------+------------\n"
                  << EvalTermLine{"Material", scores[TERM_MATERIAL][WHITE]}
                  << EvalTermLine{"Piece Sq.", scores[TERM_PIECE_SQ][WHITE]}
                  << EvalTermLine{"Pawns", scores[TERM_PAWNS][WHITE], scores[TERM_PAWNS][BLACK]}
                  << " ------------+-------------+-------------+------------\n"
                  << EvalTermLine{"Total", score} << '\n'
                  << "Evaluation: "
                  << (double(chess.turn == WHITE ? result : -result) / PAWN_VALUE_MG) << '\n';
    }

    return result;
}

template <Color c, typename Func>
void forEachPiece(U64 bitboard, Func action) {
    while (bitboard) {
        Square sq = BB::advanced<c>(bitboard);
        action(sq);
        bitboard &= BB::clear(sq);
    }
}

template <bool debug>
template <Color c>
inline Score Evaluator<debug>::pawnsEval() const {
    constexpr Color enemy = ~c;
    U64 pawns = board.getPieces<PAWN>(c);
    U64 enemyPawns = board.getPieces<PAWN>(enemy);

    Score score = {0, 0};

    U64 isolatedPawns = Eval::isolatedPawns(pawns);
    score += ISO_PAWN_PENALTY * BB::bitCount(isolatedPawns);

    U64 backwardsPawns = Eval::backwardsPawns<c>(pawns, enemyPawns);
    score += BACKWARD_PAWN_PENALTY * BB::bitCount(backwardsPawns);

    U64 doubledPawns = Eval::doubledPawns<c>(pawns);
    score += DOUBLED_PAWN_PENALTY * BB::bitCount(doubledPawns);

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
