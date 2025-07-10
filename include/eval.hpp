#pragma once

#include <array>
#include <optional>
#include <string>

#include "bb.hpp"
#include "board.hpp"
#include "constants.hpp"
#include "eval_constants.hpp"
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
    TERM_THREATS,
    N_TERMS,
};

struct EvalTermScores {
    std::optional<Score> scores[N_COLORS] = {std::nullopt};
    void addScore(Color color, Score score) { scores[color] = score; }
};

static std::ostream& operator<<(std::ostream& os, const EvalTermScores& term) {
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
class Eval {
   public:
    Eval() = delete;
    Eval(const Board&);

    int evaluate();

    using Scores = EvalConstants;

    static constexpr int KingAttackersValue[N_PIECES] = {0, 0, 50, 35, 30, 10};
    static constexpr int SafeCheckValue[N_PIECES]     = {0, 0, 600, 400, 700, 500};
    static constexpr int UnsafeCheckValue[N_PIECES]   = {0, 0, 80, 70, 60, 10};

   private:
    const Board& board;

    using TermScores = std::conditional_t<mode == Verbose, EvalTermScores, std::nullptr_t>;
    TermScores termScores[N_TERMS];

    U64 attacks[N_COLORS][N_PIECES] = {{0}};
    U64 attacksByTwo[N_COLORS]      = {0};
    U64 outposts[N_COLORS]          = {0};
    U64 mobilityZone[N_COLORS]      = {0};
    U64 kingZone[N_COLORS]          = {0};

    int kingAttackersCount[N_COLORS] = {0};
    int kingAttackersValue[N_COLORS] = {0};

    Score mobility[N_COLORS] = {ZERO_SCORE};
    Score threats[N_COLORS]  = {ZERO_SCORE};

    Score _score = ZERO_SCORE;
    int _result  = 0;

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
    int kingChecks(PieceType, U64, U64);
    template <Color>
    int kingDanger(Square);
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
    int taper(Score score);
    int nonPawnMaterial(Color) const;
    int scaleFactor() const;

    template <Verbosity m>
    friend std::ostream& operator<<(std::ostream& os, const Eval<m>& eval);

    friend class EvalTest;

    static constexpr auto PtAllIdx    = idx(PieceType::All);
    static constexpr auto PtPawnIdx   = idx(PieceType::Pawn);
    static constexpr auto PtKnightIdx = idx(PieceType::Knight);
    static constexpr auto PtBishopIdx = idx(PieceType::Bishop);
    static constexpr auto PtRookIdx   = idx(PieceType::Rook);
    static constexpr auto PtQueenIdx  = idx(PieceType::Queen);
    static constexpr auto PtKingIdx   = idx(PieceType::King);
};

template <Verbosity mode>
Eval<mode>::Eval(const Board& b) : board{b} {
    initialize<WHITE>();
    initialize<BLACK>();
}

template <Verbosity mode>
template <Color c>
void Eval<mode>::initialize() {
    constexpr Color enemy     = ~c;
    constexpr U64 rank2       = (c == WHITE) ? BB::rankBB(Rank::R2) : BB::rankBB(Rank::R7);
    constexpr U64 outpostMask = (c == WHITE) ? WHITE_OUTPOSTS : BLACK_OUTPOSTS;
    U64 pawns                 = board.pieces<PieceType::Pawn>(c);
    U64 enemyPawns            = board.pieces<PieceType::Pawn>(enemy);
    Square kingSq             = board.kingSq(c);

    attacks[c][PtAllIdx] = attacks[c][PtKingIdx] = BB::moves<PieceType::King>(kingSq);

    outposts[c] = (~BB::pawnAttackSpan<enemy>(enemyPawns) &  // cannot be attacked by enemy pawns
                   BB::pawnAttacks<c>(pawns) &               // supported by friendly pawn
                   outpostMask);                             // on file 4-6 relative to side

    mobilityZone[c] = ~(BB::pawnAttacks<enemy>(enemyPawns) |  // exclude enemy pawn attacks
                        BB::set(kingSq) |                     // king square
                        (pawns & rank2));                     // and unmoved pawns

    Square center = makeSquare(std::clamp(fileOf(kingSq), File::F2, File::F7),
                               std::clamp(rankOf(kingSq), Rank::R2, Rank::R7));
    kingZone[c]   = BB::moves<PieceType::King>(center) | BB::set(center);  // 3x3 zone around king
}

template <Verbosity mode>
template <Term term, Color c>
inline Score Eval<mode>::evaluateTerm() {
    Score score;

    if constexpr (term == TERM_MATERIAL) score = board.materialScore();
    if constexpr (term == TERM_PIECE_SQ) score = board.psqBonusScore();
    if constexpr (term == TERM_PAWNS) score = pawnsScore<c>();
    if constexpr (term == TERM_KNIGHTS) score = piecesScore<c, PieceType::Knight>();
    if constexpr (term == TERM_BISHOPS) score = piecesScore<c, PieceType::Bishop>();
    if constexpr (term == TERM_ROOKS) score = piecesScore<c, PieceType::Rook>();
    if constexpr (term == TERM_QUEENS) score = piecesScore<c, PieceType::Queen>();
    if constexpr (term == TERM_KING) score = kingScore<c>();
    if constexpr (term == TERM_MOBILITY) score = mobility[c];
    if constexpr (term == TERM_THREATS) score = threats[c];

    if constexpr (mode == Verbose) {
        termScores[term].addScore(c, score);
    }

    return score;
}

template <Verbosity mode>
int Eval<mode>::evaluate() {
    // evaluate basic terms
    _score += evaluateTerm<TERM_MATERIAL>();
    _score += evaluateTerm<TERM_PIECE_SQ>();
    _score += evaluateTerm<TERM_PAWNS, WHITE>() - evaluateTerm<TERM_PAWNS, BLACK>();
    _score += evaluateTerm<TERM_KNIGHTS, WHITE>() - evaluateTerm<TERM_KNIGHTS, BLACK>();
    _score += evaluateTerm<TERM_BISHOPS, WHITE>() - evaluateTerm<TERM_BISHOPS, BLACK>();
    _score += evaluateTerm<TERM_ROOKS, WHITE>() - evaluateTerm<TERM_ROOKS, BLACK>();
    _score += evaluateTerm<TERM_QUEENS, WHITE>() - evaluateTerm<TERM_QUEENS, BLACK>();

    // complex terms that require data from evaluating pieces
    _score    += evaluateTerm<TERM_KING, WHITE>() - evaluateTerm<TERM_KING, BLACK>();
    _score    += evaluateTerm<TERM_MOBILITY, WHITE>() - evaluateTerm<TERM_MOBILITY, BLACK>();
    _score    += evaluateTerm<TERM_THREATS, WHITE>() - evaluateTerm<TERM_THREATS, BLACK>();
    _score.eg *= scaleFactor() / float(SCALE_LIMIT);

    _result = taper(_score);
    if (board.sideToMove() == BLACK) _result = -_result;

    _result += TEMPO_BONUS;
    return _result;
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
    Score score;

    U64 pawns             = board.pieces<PieceType::Pawn>(c);
    U64 pawnAttacks       = BB::pawnAttacks<c>(pawns);
    U64 pawnDoubleAttacks = BB::pawnDoubleAttacks<c>(pawns);
    U64 enemyPawns        = board.pieces<PieceType::Pawn>(enemy);

    attacksByTwo[c]       |= pawnDoubleAttacks | (attacks[c][PtAllIdx] & pawnAttacks);
    attacks[c][PtAllIdx]  |= pawnAttacks;
    attacks[c][PtPawnIdx] |= pawnAttacks;

    // isolated pawns
    U64 pawnsFill      = BB::fillFiles(pawns);
    U64 isolatedPawns  = (pawns & ~BB::shiftWest(pawnsFill)) & (pawns & ~BB::shiftEast(pawnsFill));
    score             += Scores::IsoPawn * BB::count(isolatedPawns);

    // backwards pawns
    U64 stops           = BB::pawnMoves<PawnMove::Push, c>(pawns);
    U64 attackSpan      = BB::pawnAttackSpan<c>(pawns);
    U64 enemyAttacks    = BB::pawnAttacks<enemy>(enemyPawns);
    U64 backwardsPawns  = BB::pawnMoves<PawnMove::Push, enemy>(stops & enemyAttacks & ~attackSpan);
    score              += Scores::BackwardPawn * BB::count(backwardsPawns);

    // doubled pawns (unsupported pawn with friendly pawns behind)
    U64 pawnsBehind   = pawns & BB::spanFront<c>(pawns);
    U64 doubledPawns  = pawnsBehind & ~pawnAttacks;
    score            += Scores::DoubledPawn * BB::count(doubledPawns);

    // passed pawns
    // U64 passedPawns = pawns & ~BB::pawnFullSpan<enemy>(enemyPawns)

    return score;
}

template <Verbosity mode>
template <Color c, PieceType p>
Score Eval<mode>::piecesScore() {
    constexpr Color enemy = ~c;
    Score score;

    U64 occupied   = board.occupancy();
    U64 pawns      = board.pieces<PieceType::Pawn>(c);
    U64 enemyPawns = board.pieces<PieceType::Pawn>(enemy);

    // bonus for bishop pair
    if constexpr (p == PieceType::Bishop) {
        if (board.count(c, PieceType::Bishop) > 1) {
            score += Scores::BishopPair;
        }
    }

    forEachPiece<c>(board.pieces<p>(c), [&](Square sq) {
        U64 pieceBB = BB::set(sq);
        U64 moves   = BB::moves<p>(sq, occupied);
        if (board.blockers(c) & pieceBB) {
            moves &= BB::collinear(board.kingSq(c), sq);
        }

        attacksByTwo[c]      |= (attacks[c][PtAllIdx] & moves);
        attacks[c][PtAllIdx] |= moves;
        attacks[c][idx(p)]   |= moves;

        // populate king danger
        U64 kingAttacks = moves & kingZone[enemy];
        if (kingAttacks) {
            kingAttackersCount[c]++;
            kingAttackersValue[c] += KingAttackersValue[idx(p)];
        } else if ((p == PieceType::Bishop && (BB::moves<p>(sq, pawns) & kingZone[enemy])) |
                   (p == PieceType::Rook && (BB::moves<p>(sq) & kingZone[enemy]))) {
            score += Scores::KingZoneXrayAttack;
        }

        // populate mobility
        U64 nMoves   = BB::count(moves & mobilityZone[c]);
        mobility[c] += Scores::Mobility[idx(p)][nMoves];

        // populate threats
        U64 defenders = board.attacksTo(sq, c);
        U64 attackers = board.attacksTo(sq, enemy);
        if (BB::count(attackers) > BB::count(defenders)) {
            threats[c] += Scores::WeakPiece[idx(p)];
        }

        if constexpr (p == PieceType::Knight || p == PieceType::Bishop) {
            // bonus for minor piece outposts,  reachable knight outposts
            if (pieceBB & outposts[c]) {
                if constexpr (p == PieceType::Knight) score += Scores::KnightOutpost;
                if constexpr (p == PieceType::Bishop) score += Scores::BishopOutpost;
            } else if (p == PieceType::Knight && moves & outposts[c]) {
                score += Scores::ReachableOutpost;
            }

            // bonus minor piece guarded by pawn
            if (pieceBB & BB::pawnMoves<PawnMove::Push, enemy>(pawns)) {
                score += Scores::MinorPawnShield;
            }

            if constexpr (p == PieceType::Bishop) {
                // bonus for bishop on long diagonal
                if (BB::isMany(CENTER_SQUARES & BB::moves<p>(sq, pawns))) {
                    score += Scores::BishopLongDiagonal;
                }

                // penalty for bishop blocked by friendly pawns
                score += Scores::BishopBlockedByPawn * bishopPawnBlockers<c>(pieceBB);
            }
        }

        if constexpr (p == PieceType::Rook) {
            // bonus for rook on open file, penalty for closed file w blocked pawn
            U64 fileBB = BB::fileBB(fileOf(sq));
            if (!(pawns & fileBB)) {
                score += Scores::RookOpenFile[!(enemyPawns & fileBB)];
            } else if (pawns & fileBB & BB::pawnMoves<PawnMove::Push, enemy>(occupied)) {
                score += Scores::RookClosedFile;
            }
        }

        if constexpr (p == PieceType::Queen) {
            // penalty for discovered attacks on queen
            if (discoveredAttackOnQueen<c>(sq, occupied)) {
                score += Scores::QueenDiscoveredAttack;
            }
        }
    });

    return score;
}

template <Verbosity mode>
template <Color c>
inline Score Eval<mode>::kingScore() {
    constexpr Color enemy = ~c;
    Square kingSq         = board.kingSq(c);
    Score score           = kingShelter<c>(kingSq);

    if (board.canCastleOO(c)) score = std::max(score, kingShelter<c>(KingDestinationOO[c]));
    if (board.canCastleOOO(c)) score = std::max(score, kingShelter<c>(KingDestinationOOO[c]));

    int danger  = kingDanger<c>(kingSq);
    score      -= {danger * danger / 2048,  // scale mg quadratically
                   danger / 8};             // scale eg linearly

    return score;
}

template <Verbosity mode>
inline int Eval<mode>::kingChecks(PieceType pieceType, U64 safeChecks, U64 allChecks) {
    int count = safeChecks ? BB::count(safeChecks) : BB::count(allChecks);
    auto pt   = idx(pieceType);
    int value = safeChecks ? SafeCheckValue[pt] : UnsafeCheckValue[pt];
    return (value * (count + 1)) / 2;
}

template <Verbosity mode>
template <Color c>
inline int Eval<mode>::kingDanger(Square kingSq) {
    constexpr Color enemy = ~c;
    int danger            = 0;

    // calculate safe, potential checking squares
    U64 kqOnlyDefense   = ~attacks[c][PtAllIdx] | attacks[c][PtKingIdx] | attacks[c][PtQueenIdx];
    U64 weaklyDefended  = kqOnlyDefense & attacks[enemy][PtAllIdx] & ~attacksByTwo[c];
    U64 safeChecks      = ~attacks[c][PtAllIdx] | (weaklyDefended & attacksByTwo[enemy]);
    safeChecks         &= ~board.pieces<PieceType::All>(enemy);

    // evaluate knight checks
    U64 knightChecks      = BB::moves<PieceType::Knight>(kingSq) & attacks[c][PtKnightIdx];
    U64 safeKnightChecks  = knightChecks & safeChecks;
    danger               += kingChecks(PieceType::Knight, safeKnightChecks, knightChecks);

    // calculate sliding
    U64 straightChecks = BB::moves<PieceType::Rook>(kingSq, board.pieces<PieceType::All>());
    U64 diagonalChecks = BB::moves<PieceType::Bishop>(kingSq, board.pieces<PieceType::All>());

    // evaluate rook checks
    U64 rookChecks      = straightChecks & attacks[enemy][PtRookIdx];
    U64 safeRookChecks  = rookChecks & safeChecks;
    danger             += kingChecks(PieceType::Rook, safeRookChecks, rookChecks);

    // evaluate queen checks
    U64 queenChecks      = (straightChecks | diagonalChecks) & attacks[enemy][PtQueenIdx];
    U64 safeQueenChecks  = queenChecks & safeChecks & ~(rookChecks | attacks[c][PtQueenIdx]);
    danger              += kingChecks(PieceType::Queen, safeQueenChecks, queenChecks);

    // evaluate bishop checks
    U64 bishopChecks      = diagonalChecks & attacks[enemy][PtBishopIdx];
    U64 safeBishopChecks  = bishopChecks & safeChecks & ~queenChecks;
    danger               += kingChecks(PieceType::Bishop, safeBishopChecks, bishopChecks);

    // king zone attacks
    danger += kingAttackersValue[enemy] * kingAttackersCount[enemy];

    // king zone weaknesses
    danger += 150 * BB::count(kingZone[c] & weaklyDefended);

    // pinned pieces
    danger += 50 * BB::count(board.blockers(c));

    return danger;
}

template <Verbosity mode>
template <Color c>
inline Score Eval<mode>::kingShelter(Square kingSq) {
    constexpr Color enemy = ~c;

    File kingFile = fileOf(kingSq);
    Rank kingRank = rankOf(kingSq);

    U64 pawnsInFront = BB::spanFront<c>(BB::rankBB(kingRank));
    U64 enemyPawns   = board.pieces<PieceType::Pawn>(enemy) & pawnsInFront;
    U64 pawns =
        board.pieces<PieceType::Pawn>(c) & pawnsInFront & ~BB::pawnAttacks<enemy>(enemyPawns);

    File file   = std::clamp(kingFile, File::F2, File::F7);
    Score score = fileShelter<c>(pawns, enemyPawns, file - 1) +
                  fileShelter<c>(pawns, enemyPawns, file) +
                  fileShelter<c>(pawns, enemyPawns, file + 1);

    score += Scores::KingFile[int(kingFile)];

    bool friendlyOpenFile  = !(board.pieces<PieceType::Pawn>(c) & BB::fileBB(kingFile));
    bool enemyOpenFile     = !(board.pieces<PieceType::Pawn>(enemy) & BB::fileBB(kingFile));
    score                 += Scores::KingOpenFile[friendlyOpenFile][enemyOpenFile];

    return score;
}

template <Verbosity mode>
template <Color c>
inline Score Eval<mode>::fileShelter(U64 pawns, U64 enemyPawns, File file) {
    constexpr Color enemy = ~c;
    Score score;

    // our pawns
    pawns     &= BB::fileBB(file);
    Rank rank  = pawns ? relativeRank(BB::advancedSq<enemy>(pawns), c) : Rank::R1;
    score     += Scores::PawnRankShelter[int(rank)];

    // enemy pawns
    enemyPawns     &= BB::fileBB(file);
    Rank enemyRank  = enemyPawns ? relativeRank(BB::advancedSq<enemy>(enemyPawns), c) : Rank::R1;
    bool isBlocked  = (rank != Rank::R1) && ((rank + 1) == enemyRank);
    score          += Scores::PawnRankStorm[isBlocked][int(enemyRank)];

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
 * @param occupied The current occupancy bitboard indicating all occupied squares.
 * @return true if a discovered attack on the queen is possible, false otherwise.
 */
template <Verbosity mode>
template <Color c>
inline bool Eval<mode>::discoveredAttackOnQueen(Square sq, U64 occupied) const {
    constexpr Color enemy = ~c;
    bool attacked         = false;

    U64 attackingBishops =
        BB::moves<PieceType::Bishop>(sq, 0) & board.pieces<PieceType::Bishop>(enemy);
    forEachPiece<c>(attackingBishops, [&](Square bishopSq) {
        U64 piecesBetween = BB::between(sq, bishopSq) & occupied;
        if (piecesBetween && !BB::isMany(piecesBetween)) {
            attacked = true;
        }
    });

    U64 attackingRooks = BB::moves<PieceType::Rook>(sq, 0) & board.pieces<PieceType::Rook>(enemy);
    forEachPiece<c>(attackingRooks, [&](Square rookSq) {
        U64 piecesBetween = BB::between(sq, rookSq) & occupied;
        if (piecesBetween && !BB::isMany(piecesBetween)) {
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
    U64 pawns             = board.pieces<PieceType::Pawn>(c);
    U64 blockedPawns      = pawns & BB::pawnMoves<PawnMove::Push, enemy>(board.occupancy());
    U64 sameColorSquares  = (bb & DARK_SQUARES) ? DARK_SQUARES : LIGHT_SQUARES;
    int pawnFactor = BB::count(blockedPawns & CENTER_FILES) + !(BB::pawnAttacks<c>(pawns) & bb);
    return pawnFactor * BB::count(pawns & sameColorSquares);
}

template <Verbosity mode>
inline int Eval<mode>::phase() const {
    int npm      = nonPawnMaterial(WHITE) + nonPawnMaterial(BLACK);
    int material = std::clamp(npm, EG_LIMIT, MG_LIMIT);
    return ((material - EG_LIMIT) * PHASE_LIMIT) / (MG_LIMIT - EG_LIMIT);
}

template <Verbosity mode>
inline int Eval<mode>::taper(Score score) {
    int mgPhase = phase();
    int egPhase = PHASE_LIMIT - mgPhase;
    return ((score.mg * mgPhase) + (score.eg * egPhase)) / PHASE_LIMIT;
}

template <Verbosity mode>
inline int Eval<mode>::nonPawnMaterial(Color c) const {
    return ((board.count(c, PieceType::Knight) * KNIGHT_VALUE_MG) +
            (board.count(c, PieceType::Bishop) * BISHOP_VALUE_MG) +
            (board.count(c, PieceType::Rook) * ROOK_VALUE_MG) +
            (board.count(c, PieceType::Queen) * QUEEN_VALUE_MG));
}

template <Verbosity mode>
int Eval<mode>::scaleFactor() const {
    // place holder, scale proportionally with pawns
    int pawnCount = board.count(board.sideToMove(), PieceType::Pawn);
    return std::min(SCALE_LIMIT, 36 + 5 * pawnCount);
}

template <Verbosity mode>
std::ostream& operator<<(std::ostream& os, const Eval<mode>& eval) {
    int result = eval.board.sideToMove() == WHITE ? eval._result : -eval._result;

    os << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2);
    os << "     Term    |    White    |    Black    |    Total   \n";
    os << "             |   MG    EG  |   MG    EG  |   MG    EG \n";
    os << " ------------+-------------+-------------+------------\n";
    os << std::setw(12) << "Material" << eval.termScores[TERM_MATERIAL];
    os << std::setw(12) << "Piece Sq." << eval.termScores[TERM_PIECE_SQ];
    os << std::setw(12) << "Pawns" << eval.termScores[TERM_PAWNS];
    os << std::setw(12) << "Knights" << eval.termScores[TERM_KNIGHTS];
    os << std::setw(12) << "Bishops" << eval.termScores[TERM_BISHOPS];
    os << std::setw(12) << "Rooks" << eval.termScores[TERM_ROOKS];
    os << std::setw(12) << "Queens" << eval.termScores[TERM_QUEENS];
    os << std::setw(12) << "Kings" << eval.termScores[TERM_KING];
    os << std::setw(12) << "Mobility" << eval.termScores[TERM_MOBILITY];
    os << std::setw(12) << "Threats" << eval.termScores[TERM_THREATS];
    os << " ------------+-------------+-------------+------------\n";
    os << std::setw(12) << "Total" << EvalTermScores{{std::nullopt, eval._score}};
    os << "\nEvaluation:\t" << double(result) / PAWN_VALUE_MG << '\n';

    return os;
}

template <Verbosity mode = Silent>
inline int eval(const Board& board) {
    return Eval<mode>(board).evaluate();
}

template int eval<Silent>(const Board&);
template int eval<Verbose>(const Board&);
