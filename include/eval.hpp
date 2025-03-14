#pragma once

#include <array>
#include <optional>
#include <string>

#include "bb.hpp"
#include "chess.hpp"
#include "constants.hpp"
#include "score.hpp"
#include "types.hpp"

enum Verbosity { Verbose, Silent };

enum Term {
    TERM_MATERIAL,
    TERM_PIECE_SQ,
    TERM_PAWNS,
    TERM_KNIGHTS,
    TERM_BISHOPS,
    TERM_ROOKS,
    TERM_QUEENS,
    TERM_KING,
    TERM_MOBILITY,
    N_TERMS,
};

struct TermScores {
    std::optional<Score> scores[N_COLORS] = {std::nullopt};

    void add(Color color, Score score) { scores[color] = score; }
};

template <Verbosity mode = Silent>
class Eval {
   public:
    Eval() = delete;
    Eval(const Chess& c);
    int evaluate();

   private:
    const Chess& chess;

    using TermInfo = typename std::conditional<mode == Verbose, TermScores, std::nullptr_t>::type;
    TermInfo termInfo[N_TERMS];

    U64 attacks[N_COLORS][N_PIECES] = {};
    U64 outposts[N_COLORS] = {0, 0};
    U64 mobilityArea[N_COLORS] = {0, 0};
    U64 kingZone[N_COLORS] = {0, 0};

    Score kingDanger[N_COLORS] = {{0, 0}, {0, 0}};
    Score mobility[N_COLORS] = {{0, 0}, {0, 0}};

    template <Color>
    void initialize();

    template <Term, Color = WHITE>
    Score evaluateTerm();

    template <Color>
    Score pawnsScore();
    template <Color, PieceType>
    Score piecesScore();
    template <Color>
    Score kingScore();

    // king safety score helpers
    template <Color>
    Score kingShelter(Square);
    template <Color>
    Score fileShelter(U64, U64, File);

    // pieces score helpers
    template <Color>
    int bishopPawnBlockers(U64) const;
    template <Color>
    bool discoveredAttackOnQueen(Square, U64) const;

    // main eval helpers
    int phase() const;
    int nonPawnMaterial(Color) const;
    int scaleFactor() const;

    void printEval(double, Score) const;

    friend class EvaluatorTest;
};

template <Verbosity mode>
Eval<mode>::Eval(const Chess& c) : chess{c} {
    initialize<WHITE>();
    initialize<BLACK>();
}

template <Verbosity mode>
template <Color c>
void Eval<mode>::initialize() {
    constexpr Color enemy = ~c;
    constexpr U64 rank2 = (c == WHITE) ? BB::rank(RANK2) : BB::rank(RANK7);
    constexpr U64 outpostMask = (c == WHITE) ? WHITE_OUTPOSTS : BLACK_OUTPOSTS;
    U64 pawns = chess.pieces<PAWN>(c);
    U64 enemyPawns = chess.pieces<PAWN>(enemy);

    outposts[c] = (~BB::pawnAttackSpan<enemy>(enemyPawns)  // cannot be attacked by enemy pawns
                   & BB::pawnAttacks<c>(pawns)             // supported by friendly pawn
                   & outpostMask);

    mobilityArea[c] = ~((pawns & rank2) | BB::pawnAttacks<enemy>(enemyPawns));

    Square kingSq = chess.kingSq(c);
    Square center = makeSquare(std::clamp(fileOf(kingSq), FILE2, FILE7),
                               std::clamp(rankOf(kingSq), RANK2, RANK7));
    kingZone[c] = BB::pieceMoves<KING>(center) | BB::set(center);
}

template <Verbosity mode>
template <Term term, Color c>
inline Score Eval<mode>::evaluateTerm() {
    Score score;

    if constexpr (term == TERM_MATERIAL) score = chess.materialScore();
    if constexpr (term == TERM_PIECE_SQ) score = chess.psqBonusScore();
    if constexpr (term == TERM_PAWNS) score = pawnsScore<c>();
    if constexpr (term == TERM_KNIGHTS) score = piecesScore<c, KNIGHT>();
    if constexpr (term == TERM_BISHOPS) score = piecesScore<c, BISHOP>();
    if constexpr (term == TERM_ROOKS) score = piecesScore<c, ROOK>();
    if constexpr (term == TERM_QUEENS) score = piecesScore<c, QUEEN>();
    if constexpr (term == TERM_KING) score = kingScore<c>();
    if constexpr (term == TERM_MOBILITY) score = mobility[c];

    if constexpr (mode == Verbose) {
        termInfo[term].add(c, score);
    }

    return score;
}

template <Verbosity mode>
int Eval<mode>::evaluate() {
    Score score{0, 0};

    // evaluate basic terms
    score += evaluateTerm<TERM_MATERIAL>();
    score += evaluateTerm<TERM_PIECE_SQ>();
    score += evaluateTerm<TERM_PAWNS, WHITE>() - evaluateTerm<TERM_PAWNS, BLACK>();
    score += evaluateTerm<TERM_KNIGHTS, WHITE>() - evaluateTerm<TERM_KNIGHTS, BLACK>();
    score += evaluateTerm<TERM_BISHOPS, WHITE>() - evaluateTerm<TERM_BISHOPS, BLACK>();
    score += evaluateTerm<TERM_ROOKS, WHITE>() - evaluateTerm<TERM_ROOKS, BLACK>();
    score += evaluateTerm<TERM_QUEENS, WHITE>() - evaluateTerm<TERM_QUEENS, BLACK>();
    score += evaluateTerm<TERM_KING, WHITE>() - evaluateTerm<TERM_KING, BLACK>();

    // more complex terms that require data from evaluating pieces
    score += evaluateTerm<TERM_MOBILITY, WHITE>() - evaluateTerm<TERM_MOBILITY, BLACK>();

    // scale eg and taper mg/eg to produce final eval
    score.eg *= scaleFactor() / float(SCALE_LIMIT);
    int result = score.taper(phase());

    result = (chess.sideToMove() == WHITE)  // result relative to side to move + tempo
                 ? result + TEMPO_BONUS
                 : -result - TEMPO_BONUS;

    if constexpr (mode == Verbose) printEval(result, score);

    return result;
}

template <Color c, typename Func>
void forEachPiece(U64 bitboard, Func action) {
    while (bitboard) {
        Square sq = BB::advancedSq<c>(bitboard);
        action(sq);
        bitboard &= BB::clear(sq);
    }
}

template <Verbosity mode>
template <Color c>
inline Score Eval<mode>::pawnsScore() {
    constexpr Color enemy = ~c;
    Score score = {0, 0};
    U64 pawns = chess.pieces<PAWN>(c);
    U64 pawnAttacks = BB::pawnAttacks<c>(pawns);
    U64 enemyPawns = chess.pieces<PAWN>(enemy);

    attacks[c][ALL_PIECES] |= pawnAttacks;
    attacks[c][PAWN] |= pawnAttacks;
    kingDanger[enemy] += KING_DANGER[PAWN] * BB::count(pawnAttacks & kingZone[enemy]);

    // isolated pawns
    U64 pawnsFill = BB::fillFiles(pawns);
    U64 isolatedPawns = (pawns & ~BB::shiftWest(pawnsFill)) & (pawns & ~BB::shiftEast(pawnsFill));
    score += ISO_PAWN_PENALTY * BB::count(isolatedPawns);

    // backwards pawns
    U64 stops = BB::pawnMoves<PUSH, c>(pawns);
    U64 attackSpan = BB::pawnAttackSpan<c>(pawns);
    U64 enemyAttacks = BB::pawnAttacks<enemy>(enemyPawns);
    U64 backwardsPawns = BB::pawnMoves<PUSH, enemy>(stops & enemyAttacks & ~attackSpan);
    score += BACKWARD_PAWN_PENALTY * BB::count(backwardsPawns);

    // doubled pawns (unsupported pawn with friendly pawns behind)
    U64 pawnsBehind = pawns & BB::spanFront<c>(pawns);
    U64 supported = BB::pawnAttacks<c>(pawns);
    U64 doubledPawns = pawnsBehind & ~supported;
    score += DOUBLED_PAWN_PENALTY * BB::count(doubledPawns);

    // passed pawns
    // U64 passedPawns = pawns & ~BB::pawnFullSpan<enemy>(enemyPawns)

    return score;
}

template <Verbosity mode>
template <Color c, PieceType p>
Score Eval<mode>::piecesScore() {
    constexpr Color enemy = ~c;
    Score score = {0, 0};
    U64 occ = chess.occupancy();
    U64 pawns = chess.pieces<PAWN>(c);
    U64 enemyPawns = chess.pieces<PAWN>(enemy);

    // bonus for bishop pair
    if constexpr (p == BISHOP) {
        if (chess.count<BISHOP>(c) > 1) {
            score += BISHOP_PAIR_BONUS;
        }
    }

    forEachPiece<c>(chess.pieces<p>(c), [&](Square sq) {
        U64 bb = BB::set(sq);
        U64 moves = BB::pieceMoves<p>(sq, occ);

        attacks[c][ALL_PIECES] |= moves;
        attacks[c][p] |= moves;

        kingDanger[enemy] += KING_DANGER[p] * BB::count(moves & kingZone[enemy]);

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
            U64 fileBB = BB::file(fileOf(sq));
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

template <Verbosity mode>
template <Color c>
inline Score Eval<mode>::kingScore() {
    Square kingSq = chess.kingSq(c);
    Score score = kingShelter<c>(kingSq);

    if (chess.canCastleOO(c)) {
        score = std::max(score, kingShelter<c>(KingDestinationOO[c]));
    }
    if (chess.canCastleOOO(c)) {
        score = std::max(score, kingShelter<c>(KingDestinationOOO[c]));
    }

    score += kingDanger[c];

    return score;
}

template <Verbosity mode>
template <Color c>
inline Score Eval<mode>::kingShelter(Square kingSq) {
    constexpr Color enemy = ~c;

    File kingFile = fileOf(kingSq);
    Rank kingRank = rankOf(kingSq);

    U64 pawnsInFront = BB::spanFront<c>(BB::rank(kingRank));
    U64 enemyPawns = chess.pieces<PAWN>(enemy) & pawnsInFront;
    U64 pawns = chess.pieces<PAWN>(c) & pawnsInFront & ~BB::pawnAttacks<enemy>(enemyPawns);

    File file = std::clamp(kingFile, FILE2, FILE7);
    Score score = fileShelter<c>(pawns, enemyPawns, file - 1) +
                  fileShelter<c>(pawns, enemyPawns, file) +
                  fileShelter<c>(pawns, enemyPawns, file + 1);

    score += KING_FILE_BONUS[kingFile];

    bool friendlyOpenFile = !(chess.pieces<PAWN>(c) & BB::file(kingFile));
    bool enemyOpenFile = !(chess.pieces<PAWN>(enemy) & BB::file(kingFile));
    score += KING_OPEN_FILE_BONUS[friendlyOpenFile][enemyOpenFile];

    return score;
}

template <Verbosity mode>
template <Color c>
inline Score Eval<mode>::fileShelter(U64 pawns, U64 enemyPawns, File file) {
    constexpr Color enemy = ~c;

    Score score = {0, 0};

    U64 bb = pawns & BB::file(file);
    Rank rank = bb ? relativeRank(BB::advancedSq<enemy>(bb), c) : RANK1;
    score += PAWN_SHELTER_BONUS[rank];

    bb = enemyPawns & BB::file(file);
    Rank enemyRank = bb ? relativeRank(BB::advancedSq<enemy>(bb), c) : RANK1;
    score += (rank && (rank + 1) == enemyRank) ? BLOCKED_STORM_PENALTY[enemyRank]
                                               : PAWN_STORM_PENALTY[enemyRank];

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
template <Verbosity mode>
template <Color c>
inline bool Eval<mode>::discoveredAttackOnQueen(Square sq, U64 occ) const {
    constexpr Color enemy = ~c;
    bool attacked = false;

    U64 attackingBishops = BB::pieceMoves<BISHOP>(sq, 0) & chess.pieces<BISHOP>(enemy);
    forEachPiece<c>(attackingBishops, [&](Square bishopSq) {
        U64 piecesBetween = BB::betweenBB(sq, bishopSq) & occ;
        if (piecesBetween && !BB::hasMoreThanOne(piecesBetween)) {
            attacked = true;
        }
    });

    U64 attackingRooks = BB::pieceMoves<ROOK>(sq, 0) & chess.pieces<ROOK>(enemy);
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
template <Verbosity mode>
template <Color c>
inline int Eval<mode>::bishopPawnBlockers(U64 bb) const {
    constexpr Color enemy = ~c;
    U64 pawns = chess.pieces<PAWN>(c);
    U64 blockedPawns = pawns & BB::pawnMoves<PUSH, enemy>(chess.occupancy());
    U64 sameColorSquares = (bb & DARK_SQUARES) ? DARK_SQUARES : LIGHT_SQUARES;
    int pawnFactor = BB::count(blockedPawns & CENTER_FILES) + !(BB::pawnAttacks<c>(pawns) & bb);
    return pawnFactor * BB::count(pawns & sameColorSquares);
}

template <Verbosity mode>
inline int Eval<mode>::phase() const {
    int npm = nonPawnMaterial(WHITE) + nonPawnMaterial(BLACK);
    int material = std::clamp(npm, EG_LIMIT, MG_LIMIT);
    return ((material - EG_LIMIT) * PHASE_LIMIT) / (MG_LIMIT - EG_LIMIT);
}

template <Verbosity mode>
inline int Eval<mode>::nonPawnMaterial(Color c) const {
    return (chess.count<KNIGHT>(c) * KNIGHT_VALUE_MG + chess.count<BISHOP>(c) * BISHOP_VALUE_MG +
            chess.count<ROOK>(c) * ROOK_VALUE_MG + chess.count<QUEEN>(c) * QUEEN_VALUE_MG);
}

template <Verbosity mode>
int Eval<mode>::scaleFactor() const {
    // place holder, scale proportionally with pawns
    int pawnCount = chess.count<PAWN>(chess.sideToMove());
    return std::min(SCALE_LIMIT, 36 + 5 * pawnCount);
}

template <Verbosity mode>
void Eval<mode>::printEval(double result, Score score) const {
    result = chess.sideToMove() == WHITE ? result : -result;
    // clang-format off
    std::cout << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2)
                << "     Term    |    White    |    Black    |    Total   \n"
                << "             |   MG    EG  |   MG    EG  |   MG    EG \n"
                << " ------------+-------------+-------------+------------\n"
                << std::setw(12) << "Material" << termInfo[TERM_MATERIAL]
                << std::setw(12) << "Piece Sq." << termInfo[TERM_PIECE_SQ]
                << std::setw(12) << "Pawns" << termInfo[TERM_PAWNS]
                << std::setw(12) << "Knights" << termInfo[TERM_KNIGHTS]
                << std::setw(12) << "Bishops" << termInfo[TERM_BISHOPS]
                << std::setw(12) << "Rooks" << termInfo[TERM_ROOKS]
                << std::setw(12) << "Queens" << termInfo[TERM_QUEENS]
                << std::setw(12) << "Kings" << termInfo[TERM_KING]
                << std::setw(12) << "Mobility" << termInfo[TERM_MOBILITY]
                << " ------------+-------------+-------------+------------\n"
                << std::setw(12) << "Total" << TermScores{{std::nullopt, score}}
                << "\nEvaluation:\t" << result / PAWN_VALUE_MG << '\n';
    // clang-format on
}

inline std::ostream& operator<<(std::ostream& os, const TermScores& term) {
    os << " | ";
    if (term.scores[BLACK].has_value()) {
        os << term.scores[WHITE].value() << " | ";
        os << term.scores[BLACK].value() << " | ";
        os << term.scores[WHITE].value() - term.scores[BLACK].value();
    } else {
        os << " ----  ---- |  ----  ---- | " << term.scores[WHITE].value();
    }
    os << '\n';
    return os;
}

template <Verbosity mode = Silent>
int eval(const Chess& chess) {
    return Eval<mode>(chess).evaluate();
}
template int eval<Silent>(const Chess&);
template int eval<Verbose>(const Chess&);
