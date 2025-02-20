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
    TERM_KING_SAFETY,
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

    U64 outposts[N_COLORS] = {0, 0};
    U64 mobilityArea[N_COLORS] = {0, 0};
    U64 kingArea[N_COLORS] = {0, 0};

    Score mobility[N_COLORS] = {{0, 0}, {0, 0}};

    using ScoresDetail =
        typename std::conditional<debug, Score[N_TERMS][N_COLORS], std::nullptr_t>::type;
    ScoresDetail scores{};

    template <Color>
    void initialize();

    template <Term, Color = WHITE>
    Score evaluateTerm();

    template <Color>
    Score pawnsScore();
    template <Color>
    Score kingSafetyScore();
    template <Color, PieceType>
    Score piecesScore();

    friend class EvaluatorTest;

    template <Color>
    int bishopPawnBlockers(U64) const;
    template <Color>
    bool discoveredAttackOnQueen(Square, U64) const;

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
    mobilityArea[c] = ~((pawns & BB::rank(RANK2, c)) | BB::pawnAttacks<enemy>(enemyPawns));
    kingArea[c] = BB::pieceMoves<KING>(board.kingSq[c]);
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
    } else if constexpr (term == TERM_ROOKS) {
        score = piecesScore<c, ROOK>();
    } else if constexpr (term == TERM_QUEENS) {
        score = piecesScore<c, QUEEN>();
    } else if constexpr (term == TERM_KING_SAFETY) {
        score = kingSafetyScore<c>();
    } else if constexpr (term == TERM_MOBILITY) {
        score = mobility[c];
    } else {
        score = Score{0};
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
    score += evaluateTerm<TERM_ROOKS, WHITE>() - evaluateTerm<TERM_ROOKS, BLACK>();
    score += evaluateTerm<TERM_QUEENS, WHITE>() - evaluateTerm<TERM_QUEENS, BLACK>();
    score += evaluateTerm<TERM_KING_SAFETY, WHITE>() - evaluateTerm<TERM_KING_SAFETY, BLACK>();

    // more complex eval terms require pieces to be evaluated first
    score += evaluateTerm<TERM_MOBILITY, WHITE>() - evaluateTerm<TERM_MOBILITY, BLACK>();

    // scale eg and taper mg/eg to produce final eval
    score.eg *= scaleFactor() / float(SCALE_LIMIT);
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
                  << TermOutput{"Rooks", scores[TERM_ROOKS], std::nullopt}
                  << TermOutput{"Queens", scores[TERM_QUEENS], std::nullopt}
                  << TermOutput{"Queens", scores[TERM_QUEENS], std::nullopt}
                  << TermOutput{"Mobility", scores[TERM_MOBILITY], std::nullopt}
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
inline Score Evaluator<debug>::pawnsScore() {
    constexpr Color enemy = ~c;
    Score score = {0, 0};
    U64 pawns = board.getPieces<PAWN>(c);
    U64 enemyPawns = board.getPieces<PAWN>(enemy);

    U64 isolatedPawns = Eval::isolatedPawns(pawns);
    score += ISO_PAWN_PENALTY * BB::count(isolatedPawns);

    U64 backwardsPawns = Eval::backwardsPawns<c>(pawns, enemyPawns);
    score += BACKWARD_PAWN_PENALTY * BB::count(backwardsPawns);

    U64 doubledPawns = Eval::doubledPawns<c>(pawns);
    score += DOUBLED_PAWN_PENALTY * BB::count(doubledPawns);

    return score;
}

template <bool debug>
template <Color c>
inline Score Evaluator<debug>::kingSafetyScore() {
    constexpr Color enemy = ~c;
    Score score = {0, 0};

    return score;
}

template <bool debug>
template <Color c, PieceType p>
Score Evaluator<debug>::piecesScore() {
    constexpr Color enemy = ~c;
    Score score = {0, 0};
    U64 occ = board.occupancy();
    U64 pawns = board.getPieces<PAWN>(c);
    U64 enemyPawns = board.getPieces<PAWN>(enemy);

    // bonus for bishop pair
    if constexpr (p == BISHOP) {
        if (board.count<BISHOP>(c) > 1) {
            score += BISHOP_PAIR_BONUS;
        }
    }

    forEachPiece<c>(board.getPieces<p>(c), [&](Square sq) {
        U64 bb = BB::set(sq);
        U64 moves = BB::pieceMoves<p>(sq, occ);

        U64 nMoves = BB::count(moves & mobilityArea[c]);
        mobility[c] += MOBILITY_BONUS[p][nMoves];

        if constexpr (p == KNIGHT || p == BISHOP) {
            // bonus for minor piece outposts,  reachable knight outposts
            if (bb & outposts[c]) {
                score += OUTPOST_BONUS[p == KNIGHT];
            } else if (p == KNIGHT && moves & outposts[c]) {
                score += REACHABLE_OUTPOST_BONUS;
            }

            // bonus minor piece guarded by pawn
            if (bb & BB::pawnMoves<PUSH, enemy>(pawns)) {
                score += MINOR_BEHIND_PAWN_BONUS;
            }

            if constexpr (p == BISHOP) {
                // bonus for bishop on long diagonal
                if (BB::hasMoreThanOne(CENTER_SQUARES & BB::pieceMoves<p>(sq, pawns))) {
                    score += BISHOP_LONG_DIAG_BONUS;
                }

                // penalty for bishop blocked by friendly pawns
                score += BISHOP_PAWN_BLOCKER_PENALTY * bishopPawnBlockers<c>(bb);
            }
        }

        if constexpr (p == ROOK) {
            // bonus for rook on open file, penalty for closed file w blocked pawn
            U64 fileBB = BB::file(Defs::fileFromSq(sq));
            if (!(pawns & fileBB)) {
                score += ROOK_OPEN_FILE_BONUS[!(enemyPawns & fileBB)];
            } else {
                if (pawns & fileBB & BB::pawnMoves<PUSH, enemy>(occ)) {
                    score += ROOK_CLOSED_FILE_PENALTY;
                }
            }
        }

        if constexpr (p == QUEEN) {
            // penalty for discovered attacks on queen
            if (discoveredAttackOnQueen<c>(sq, occ)) {
                score += DISCOVERED_ATTACK_ON_QUEEN_PENALTY;
            }
        }
    });

    return score;
}

/**
 * @brief Determines if the queen at the given square is vulnerable to a discovered attack.
 *
 * This function evaluates whether there is an enemy bishop or rook on the same diagonal, rank, or
 * file as the specified square, such that a discovered attack on the queen could occur. A
 * discovered attack happens when there is exactly one piece between the enemy attacker and the
 * square in question, and that piece is moved to reveal the attack.
 *
 * @tparam debug A boolean flag for enabling debug mode.
 * @tparam c The color of the player to check for discovered attacks (WHITE or BLACK).
 * @param sq The square occupied by the queen that is potentially subject to a discovered attack.
 * @param occ The current occupancy bitboard indicating all occupied squares.
 * @return true if a discovered attack on the queen is possible, false otherwise.
 */
template <bool debug>
template <Color c>
inline bool Evaluator<debug>::discoveredAttackOnQueen(Square sq, U64 occ) const {
    constexpr Color enemy = ~c;
    bool attacked = false;

    U64 attackingBishops = BB::pieceMoves<BISHOP>(sq, 0) & board.getPieces<BISHOP>(enemy);
    forEachPiece<c>(attackingBishops, [&](Square bishopSq) {
        U64 piecesBetween = BB::betweenBB(sq, bishopSq) & occ;
        if (piecesBetween && !BB::hasMoreThanOne(piecesBetween)) {
            attacked = true;
        }
    });

    U64 attackingRooks = BB::pieceMoves<ROOK>(sq, 0) & board.getPieces<ROOK>(enemy);
    forEachPiece<c>(attackingRooks, [&](Square rookSq) {
        U64 piecesBetween = BB::betweenBB(sq, rookSq) & occ;
        if (piecesBetween && !BB::hasMoreThanOne(piecesBetween)) {
            attacked = true;
        }
    });

    return attacked;
}

/**
 * @brief Evaluates the alignment of the current player's pawns and bishops relative to the blocked
 * central pawns.
 *
 * This function calculates a score based on the interaction of the current player's pawns and
 * a specific bishop color (light or dark) specified by the bitboard `bb`. The score is adjusted by
 * the number of blocked pawns in the central files (C, D, E, F) and whether the bishop's path is
 * clear from friendly pawn attacks.
 *
 * @tparam debug A boolean flag for enabling debug mode.
 * @tparam c The color of the player (WHITE or BLACK).
 * @param bb A bitboard indicating the squares occupied by the player's bishops of a specific color.
 * @return An integer score representing the influence of pawns on the given bishops, adjusted by
 *         the number of blocked central pawns.
 */
template <bool debug>
template <Color c>
inline int Evaluator<debug>::bishopPawnBlockers(U64 bb) const {
    constexpr Color enemy = ~c;
    U64 pawns = board.getPieces<PAWN>(c);
    U64 blockedPawns = pawns & BB::pawnMoves<PUSH, enemy>(board.occupancy());
    U64 sameColorSquares = (bb & DARK_SQUARES) ? DARK_SQUARES : LIGHT_SQUARES;
    int pawnFactor =
        BB::count(blockedPawns & CENTER_FILES) + !(BB::pawnAttacks<c>(pawns) & bb);
    return pawnFactor * BB::count(pawns & sameColorSquares);
}

template <bool debug>
inline int Evaluator<debug>::phase() const {
    int npm = nonPawnMaterial(WHITE) + nonPawnMaterial(BLACK);
    int material = std::clamp(npm, EG_LIMIT, MG_LIMIT);
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
    // place holder, scale proportionally with pawns
    int pawnCount = board.count<PAWN>(chess.turn);
    return std::min(SCALE_LIMIT, 36 + 5 * pawnCount);
}

#endif
