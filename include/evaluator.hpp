#ifndef LATRUNCULI_EVALUATOR_H
#define LATRUNCULI_EVALUATOR_H

#include <optional>
#include <string>

#include "bb.hpp"
#include "board.hpp"
#include "chess.hpp"
#include "eval.hpp"
#include "score.hpp"
#include "types.hpp"

enum Term {
    TERM_MATERIAL,
    TERM_PIECE_SQ,
    TERM_PAWNS,
    TERM_KNIGHTS,
    TERM_BISHOPS,
    TERM_ROOKS,
    TERM_QUEENS,
    TERM_KINGS,
    // tbd
    TERM_MOBILITY,
    TERM_THREATS,
    TERM_SPACE,
    TERM_INITIATIVE,
    N_TERMS,

};

struct TermOutput {
    std::string name;
    const Score* scores;
    std::optional<Score> total;

    friend std::ostream& operator<<(std::ostream& os, const TermOutput& term) {
        os << std::setw(12) << term.name << " | ";
        if (term.total) {
            os << " ----  ---- |  ----  ---- | " << *term.total;
        } else if (term.scores) {
            os << term.scores[WHITE] << " | ";
            os << term.scores[BLACK] << " | ";
            os << term.scores[WHITE] - term.scores[BLACK];
        }
        os << '\n';
        return os;
    }
};

template <bool debug = false>
class Evaluator {
   public:
    Evaluator() = delete;
    Evaluator(const Chess& c) : chess{c}, board{c.board} {
        initialize<WHITE>();
        initialize<BLACK>();
    }
    int eval();

   private:
    const Chess& chess;
    const Board& board;

    U64 outposts[N_COLORS] = {0};

    using ScoresDetail =
        typename std::conditional<debug, Score[N_TERMS][N_COLORS], std::nullptr_t>::type;
    ScoresDetail scores{};

    template <Color>
    void initialize();

    template <Term, Color = WHITE>
    Score evaluateTerm();

    template <Color>
    Score pawnsScore() const;
    template <Color, PieceType>
    Score piecesScore() const;

    friend class EvaluatorTest;

    // old code
    template <Color>
    Score bishopEval() const;
    template <Color>
    Score queenEval() const;

    int bishopPawnBlockers() const;

    bool hasOppositeBishops() const;
    int phase() const;
    int nonPawnMaterial(Color) const;

    int scaleFactor() const;
};

template <bool debug>
template <Color c>
void Evaluator<debug>::initialize() {
    constexpr Color enemy = ~c;
    U64 pawns = board.getPieces<PAWN>(c);
    U64 enemyPawns = board.getPieces<PAWN>(enemy);
    outposts[c] = Eval::outpostSquares<c>(pawns, enemyPawns);
}

template <bool debug>
template <Term term, Color c>
inline Score Evaluator<debug>::evaluateTerm() {
    Score score;

    if constexpr (term == TERM_MATERIAL) {
        score = chess.materialScore();
    } else if constexpr (term == TERM_PIECE_SQ) {
        score = chess.psqBonusScore();
    } else if constexpr (term == TERM_PAWNS) {
        score = pawnsScore<c>();
    } else if constexpr (term == TERM_KNIGHTS) {
        score = piecesScore<c, KNIGHT>();
    } else if constexpr (term == TERM_BISHOPS) {
        score = piecesScore<c, BISHOP>();
    }

    if constexpr (debug) {
        scores[term][c] = score;
    }

    return score;
}

template <bool debug>
int Evaluator<debug>::eval() {
    Score score{0, 0};

    score += evaluateTerm<TERM_MATERIAL>();
    score += evaluateTerm<TERM_PIECE_SQ>();
    score += evaluateTerm<TERM_PAWNS, WHITE>() - evaluateTerm<TERM_PAWNS, BLACK>();
    score += evaluateTerm<TERM_KNIGHTS, WHITE>() - evaluateTerm<TERM_KNIGHTS, BLACK>();
    score += evaluateTerm<TERM_BISHOPS, WHITE>() - evaluateTerm<TERM_BISHOPS, BLACK>();

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
                  << TermOutput{"Material", nullptr, scores[TERM_MATERIAL][WHITE]}
                  << TermOutput{"Piece Sq.", nullptr, scores[TERM_PIECE_SQ][WHITE]}
                  << TermOutput{"Pawns", scores[TERM_PAWNS], std::nullopt}
                  << TermOutput{"Knights", scores[TERM_KNIGHTS], std::nullopt}
                  << TermOutput{"Bishops", scores[TERM_BISHOPS], std::nullopt}
                  << " ------------+-------------+-------------+------------\n"
                  << TermOutput{"Total", nullptr, score} << '\n'
                  << "Evaluation: \t"
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
inline Score Evaluator<debug>::pawnsScore() const {
    constexpr Color enemy = ~c;
    Score score = {0, 0};
    U64 pawns = board.getPieces<PAWN>(c);
    U64 enemyPawns = board.getPieces<PAWN>(enemy);

    U64 isolatedPawns = Eval::isolatedPawns(pawns);
    score += ISO_PAWN_PENALTY * BB::bitCount(isolatedPawns);

    U64 backwardsPawns = Eval::backwardsPawns<c>(pawns, enemyPawns);
    score += BACKWARD_PAWN_PENALTY * BB::bitCount(backwardsPawns);

    U64 doubledPawns = Eval::doubledPawns<c>(pawns);
    score += DOUBLED_PAWN_PENALTY * BB::bitCount(doubledPawns);

    return score;
}

template <bool debug>
template <Color c, PieceType p>
Score Evaluator<debug>::piecesScore() const {
    constexpr Color enemy = ~c;
    Score score = {0, 0};
    U64 occ = board.occupancy();
    U64 enemyPawns = board.getPieces<PAWN>(enemy);

    forEachPiece<c>(board.getPieces<p>(c), [&](Square sq) {
        // minor pieces
        if constexpr (p == KNIGHT || p == BISHOP) {
            // bonus for minor piece outposts, reachable knight outposts
            if (BB::set(sq) & outposts[c]) {
                score += OUTPOST_BONUS[p == KNIGHT];
            } else if (p == KNIGHT && BB::movesByPiece<p>(sq, occ) & outposts[c]) {
                score += REACHABLE_OUTPOST_BONUS;
            }

            // bonus for pawn in front of minor piece
            if (BB::set(sq) & BB::movesByPawns<PawnMove::PUSH, enemy>(board.getPieces<PAWN>(c))) {
                score += MINOR_BEHIND_PAWN_BONUS;
            }

            if constexpr (p == BISHOP) {
                if (BB::moreThanOneSet(CENTER_SQUARES &
                                       BB::movesByPiece<p>(sq, board.getPieces<PAWN>(c)))) {
                    score += BISHOP_LONG_DIAG_BONUS;
                }
            }
        }
    });

    return score;
}

// template <bool debug>
// inline Score Evaluator<debug>::piecesEval() const {
//     Score score = {0, 0};

//     score += BISHOP_PAWN_BLOCKER_PENALTY * bishopPawnBlockers();
//     score += bishopEval<WHITE>() - bishopEval<BLACK>();
//     score += queenEval<WHITE>() - queenEval<BLACK>();

//     return score;
// }

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
inline int Evaluator<debug>::bishopPawnBlockers() const {
    U64 wPawns = board.getPieces<PAWN>(WHITE);
    U64 bPawns = board.getPieces<PAWN>(BLACK);
    return (Eval::bishopPawnBlockers<WHITE>(board.getPieces<BISHOP>(WHITE), wPawns, bPawns) -
            Eval::bishopPawnBlockers<BLACK>(board.getPieces<BISHOP>(BLACK), bPawns, wPawns));
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
