#pragma once

#include <array>
#include <optional>
#include <string>

#include "bb.hpp"
#include "board.hpp"
#include "constants.hpp"
#include "eval_base.hpp"
#include "score.hpp"
#include "types.hpp"

// Evaluates chess positions using material, mobility, king safety, and positional factors
class Eval {
   public:
    using Conf         = EvalConfig;
    using TermCallback = std::function<void(EvalTerm, Color, Score)>;

    Eval() = delete;
    Eval(const Board&, TermCallback termCallback = nullptr);

    int evaluate();

    friend class EvalVerbose;

   private:
    const Board& board;
    TermCallback termCallback = nullptr;

    struct AttackData {
        U64 by[N_COLORS][N_PIECES] = {{0}};
        U64 byTwo[N_COLORS]        = {0};
    } attacks;

    struct ZoneData {
        U64 outposts[N_COLORS] = {0};
        U64 mobility[N_COLORS] = {0};
        U64 king[N_COLORS]     = {0};
    } zones;

    struct KingAttackersData {
        int count[N_COLORS] = {0};
        int value[N_COLORS] = {0};
    } kingAttackers;

    struct ScoreData {
        Score mobility[N_COLORS] = {ZERO_SCORE};
        Score threats[N_COLORS]  = {ZERO_SCORE};
        Score finalScore         = ZERO_SCORE;
        int finalResult          = 0;
    } scores;

    struct PieceContext {
        Square square  = INVALID;
        U64 pieceBB    = 0;
        U64 occupied   = 0;
        U64 pawns      = 0;
        U64 enemyPawns = 0;
    };

    // Initialization methods
    template <Color>
    void initialize();
    template <Color c>
    U64 calculateOutpostsZone(U64 pawns, U64 enemyPawns);
    template <Color c>
    U64 calculateMobilityZone(U64 pawns, U64 enemyPawns, Square kingSq);
    template <Color c>
    U64 calculateKingZone(Square kingSq);

    // Core evaluation methods
    template <EvalTerm, Color = WHITE>
    Score evaluateTerm();
    template <Color>
    Score evaluatePawns();
    template <Color, PieceType>
    Score evaluatePieces();
    template <Color>
    Score evaluateKing();

    // Update methods for accumulating data during evaluation
    template <Color c, PieceType p>
    void updateAttackBitboards(U64 moves);
    template <Color c, PieceType p>
    void updateMobilityScore(U64 moves);
    template <Color c, PieceType p>
    void updateThreatScore(const PieceContext& ctx);
    template <Color c, PieceType p>
    Score updateKingDanger(const PieceContext& ctx, U64 moves);

    // Piece specific evaluation methods
    template <Color c, PieceType p>
    U64 calculateMoves(const PieceContext& ctx);
    template <Color c, PieceType p>
    Score evaluateMinorPiece(const PieceContext& ctx, U64 moves);
    template <Color c>
    Score evaluateBishop(const PieceContext& ctx);
    template <Color>
    Score bishopPawnBlockers(const PieceContext& ctx) const;
    template <Color c>
    Score evaluateRook(const PieceContext& ctx);
    template <Color c>
    Score evaluateQueen(const PieceContext& ctx);
    template <Color c, PieceType p>
    bool discoveredAttack(const PieceContext& ctx);

    // King evaluation methods
    template <Color>
    Score kingShelter(Square kingSq);
    template <Color>
    Score kingFileShelter(U64 pawns, U64 enemyPawns, File file);
    template <Color c>
    Score kingDanger(Square kingSq);
    template <Color>
    int kingDangerRaw(Square kingSq);
    template <PieceType>
    int kingChecksDanger(U64 safeChecks, U64 allChecks);

    // Helpers
    float scaleFactor() const;
    int taperScore(Score score) const;
    int phase() const;
    int nonPawnMaterial(Color) const;

    friend class EvalTest;
};

inline Eval::Eval(const Board& b, TermCallback termCallback)
    : board{b}, termCallback{termCallback} {
    initialize<WHITE>();
    initialize<BLACK>();
}

/// returns static evaluation of the position
inline int Eval::evaluate() {
    Score score;

    // basic terms
    score += evaluateTerm<EvalTerm::Material>();
    score += evaluateTerm<EvalTerm::PieceSquares>();
    score += evaluateTerm<EvalTerm::Pawns, WHITE>() - evaluateTerm<EvalTerm::Pawns, BLACK>();
    score += evaluateTerm<EvalTerm::Knights, WHITE>() - evaluateTerm<EvalTerm::Knights, BLACK>();
    score += evaluateTerm<EvalTerm::Bishops, WHITE>() - evaluateTerm<EvalTerm::Bishops, BLACK>();
    score += evaluateTerm<EvalTerm::Rooks, WHITE>() - evaluateTerm<EvalTerm::Rooks, BLACK>();
    score += evaluateTerm<EvalTerm::Queens, WHITE>() - evaluateTerm<EvalTerm::Queens, BLACK>();

    // terms requiring accumulated data from basic terms
    score += evaluateTerm<EvalTerm::King, WHITE>() - evaluateTerm<EvalTerm::King, BLACK>();
    score += evaluateTerm<EvalTerm::Mobility, WHITE>() - evaluateTerm<EvalTerm::Mobility, BLACK>();
    score += evaluateTerm<EvalTerm::Threats, WHITE>() - evaluateTerm<EvalTerm::Threats, BLACK>();

    score.eg          *= scaleFactor();
    scores.finalScore  = score;

    int stm            = ((board.sideToMove() == WHITE) ? 1 : -1);
    scores.finalResult = taperScore(score) * stm + TEMPO_BONUS;
    return scores.finalResult;
}

/// prep zone data + seed king attacks
template <Color c>
inline void Eval::initialize() {
    constexpr Color enemy = ~c;

    Square kingSq = board.kingSq(c);
    U64 kingMoves = BB::moves<PieceType::King>(kingSq);
    updateAttackBitboards<c, PieceType::King>(kingMoves);

    U64 pawns         = board.pieces<PieceType::Pawn>(c);
    U64 enemyPawns    = board.pieces<PieceType::Pawn>(enemy);
    zones.outposts[c] = calculateOutpostsZone<c>(pawns, enemyPawns);
    zones.mobility[c] = calculateMobilityZone<c>(pawns, enemyPawns, kingSq);
    zones.king[c]     = calculateKingZone<c>(kingSq);
}

/// outposts mask: behind enemy pawns, supported by friendly pawns, on ranks 4-6
template <Color c>
inline U64 Eval::calculateOutpostsZone(U64 pawns, U64 enemyPawns) {
    constexpr Color enemy    = ~c;
    constexpr U64 rank4thru6 = (c == WHITE) ? Conf::wOutposts : Conf::bOutposts;

    U64 behindEnemy = ~BB::pawnAttackSpan<enemy>(enemyPawns);
    U64 supported   = BB::pawnAttacks<c>(pawns);
    return (behindEnemy & supported & rank4thru6);
}

/// mobility mask: safe from enemy pawns, not occupied by the king or rank 2 pawns
template <Color c>
inline U64 Eval::calculateMobilityZone(U64 pawns, U64 enemyPawns, Square kingSq) {
    constexpr Color enemy = ~c;
    constexpr U64 rank2   = (c == WHITE) ? BB::rankBB(Rank::R2) : BB::rankBB(Rank::R7);

    U64 safe     = ~BB::pawnAttacks<enemy>(enemyPawns);
    U64 occupied = BB::set(kingSq) | (pawns & rank2);
    return (safe & ~occupied);
}

// king zone: 3x3 king neighborhood for king safety evaluation
template <Color c>
inline U64 Eval::calculateKingZone(Square kingSq) {
    Square center = makeSquare(std::clamp(fileOf(kingSq), File::F2, File::F7),
                               std::clamp(rankOf(kingSq), Rank::R2, Rank::R7));
    return BB::moves<PieceType::King>(center) | BB::set(center);
}

// dispatch a single eval term -> score
template <EvalTerm term, Color c>
inline Score Eval::evaluateTerm() {
    Score score;

    // clang-format off
    switch (term) {
        case EvalTerm::Material:     score = board.materialScore(); break;
        case EvalTerm::PieceSquares: score = board.psqBonusScore(); break;
        case EvalTerm::Pawns:        score = evaluatePawns<c>(); break;
        case EvalTerm::Knights:      score = evaluatePieces<c, PieceType::Knight>(); break;
        case EvalTerm::Bishops:      score = evaluatePieces<c, PieceType::Bishop>(); break;
        case EvalTerm::Rooks:        score = evaluatePieces<c, PieceType::Rook>(); break;
        case EvalTerm::Queens:       score = evaluatePieces<c, PieceType::Queen>(); break;
        case EvalTerm::King:         score = evaluateKing<c>(); break;
        case EvalTerm::Mobility:     score = scores.mobility[c]; break;
        case EvalTerm::Threats:      score = scores.threats[c]; break;
        default: break;
    }
    // clang-format on

    if (termCallback) termCallback(term, c, score);
    return score;
}

// eval pawn structure: isolated + backward + doubled
template <Color c>
Score Eval::evaluatePawns() {
    constexpr Color enemy = ~c;
    Score score;

    U64 pawns             = board.pieces<PieceType::Pawn>(c);
    U64 enemyPawns        = board.pieces<PieceType::Pawn>(enemy);
    U64 pawnAttacks       = BB::pawnAttacks<c>(pawns);
    U64 pawnDoubleAttacks = BB::pawnDoubleAttacks<c>(pawns);

    attacks.byTwo[c] |= pawnDoubleAttacks | (attacks.by[c][PieceIdx::All] & pawnAttacks);
    attacks.by[c][PieceIdx::All]  |= pawnAttacks;
    attacks.by[c][PieceIdx::Pawn] |= pawnAttacks;

    // isolated pawns
    U64 pawnsFill      = BB::fillFiles(pawns);
    U64 isolatedPawns  = (pawns & ~BB::shiftWest(pawnsFill)) & (pawns & ~BB::shiftEast(pawnsFill));
    score             += Conf::IsoPawn * BB::count(isolatedPawns);

    // backwards pawns
    U64 stops           = BB::pawnMoves<PawnMove::Push, c>(pawns);
    U64 attackSpan      = BB::pawnAttackSpan<c>(pawns);
    U64 enemyAttacks    = BB::pawnAttacks<enemy>(enemyPawns);
    U64 backwardsPawns  = BB::pawnMoves<PawnMove::Push, enemy>(stops & enemyAttacks & ~attackSpan);
    score              += Conf::BackwardPawn * BB::count(backwardsPawns);

    // doubled pawns (unsupported pawn with friendly pawns behind)
    U64 pawnsBehind   = pawns & BB::spanFront<c>(pawns);
    U64 doubledPawns  = pawnsBehind & ~pawnAttacks;
    score            += Conf::DoubledPawn * BB::count(doubledPawns);

    return score;
}

// eval all pieces of type p for color c
template <Color c, PieceType p>
Score Eval::evaluatePieces() {
    constexpr Color enemy = ~c;
    Score score;

    U64 occupied   = board.occupancy();
    U64 pawns      = board.pieces<PieceType::Pawn>(c);
    U64 enemyPawns = board.pieces<PieceType::Pawn>(enemy);

    U64 pieceBB = board.pieces<p>(c);
    BB::scan<c>(pieceBB, [&](Square sq) {
        PieceContext context{.square     = sq,
                             .pieceBB    = BB::set(sq),
                             .occupied   = occupied,
                             .pawns      = pawns,
                             .enemyPawns = enemyPawns};

        U64 moves = calculateMoves<c, p>(context);
        updateAttackBitboards<c, p>(moves);
        updateMobilityScore<c, p>(moves);
        updateThreatScore<c, p>(context);
        score += updateKingDanger<c, p>(context, moves);

        if constexpr (p == PieceType::Bishop || p == PieceType::Knight) {
            score += evaluateMinorPiece<c, p>(context, moves);
        }

        if constexpr (p == PieceType::Bishop) {
            score += evaluateBishop<c>(context);
        } else if constexpr (p == PieceType::Rook) {
            score += evaluateRook<c>(context);
        } else if constexpr (p == PieceType::Queen) {
            score += evaluateQueen<c>(context);
        }
    });

    if constexpr (p == PieceType::Bishop) {
        if (board.count(c, PieceType::Bishop) > 1) score += Conf::BishopPair;
    }

    return score;
}

// king safety: pawn shelter + incoming attacks
template <Color c>
Score Eval::evaluateKing() {
    constexpr Color enemy = ~c;
    Score score;
    Square kingSq = board.kingSq(c);

    score += kingShelter<c>(kingSq);
    if (board.canCastleOO(c)) score = std::max(score, kingShelter<c>(KingDestinationOO[c]));
    if (board.canCastleOOO(c)) score = std::max(score, kingShelter<c>(KingDestinationOOO[c]));

    score -= kingDanger<c>(kingSq);

    return score;
}

// merge moves into attack bitboards
template <Color c, PieceType p>
inline void Eval::updateAttackBitboards(U64 moves) {
    attacks.byTwo[c]             |= (attacks.by[c][PieceIdx::All] & moves);
    attacks.by[c][PieceIdx::All] |= moves;
    attacks.by[c][idx(p)]        |= moves;
}

// add mobility bonus for # of moves
template <Color c, PieceType p>
inline void Eval::updateMobilityScore(U64 moves) {
    U64 moveCount       = BB::count(moves & zones.mobility[c]);
    scores.mobility[c] += Conf::Mobility[idx(p)][moveCount];
}

// penalize weak pieces if attackers > defenders
template <Color c, PieceType p>
inline void Eval::updateThreatScore(const PieceContext& ctx) {
    constexpr Color enemy = ~c;

    U64 defenders    = board.attacksTo(ctx.square, c);
    U64 attackers    = board.attacksTo(ctx.square, enemy);
    bool outnumbered = BB::count(attackers) > BB::count(defenders);
    if (outnumbered) {
        scores.threats[c] += Conf::WeakPiece[idx(p)];
    }
}

// update king attackers with attacks on enemy king zone
template <Color c, PieceType p>
inline Score Eval::updateKingDanger(const PieceContext& ctx, U64 moves) {
    constexpr Color enemy = ~c;

    if (moves & zones.king[enemy]) {
        kingAttackers.count[enemy]++;
        kingAttackers.value[enemy] += Conf::AttackedKingZoneDanger[idx(p)];
    } else if constexpr (p == PieceType::Bishop || p == PieceType::Rook) {
        U64 xrays = BB::moves<p>(ctx.square, ctx.pawns) & zones.king[enemy];
        if (xrays) return Conf::KingZoneXrayAttack;
    }

    return ZERO_SCORE;
}

template <Color c, PieceType p>
inline U64 Eval::calculateMoves(const PieceContext& ctx) {
    U64 moves       = BB::moves<p>(ctx.square, ctx.occupied);
    U64 pinnedPiece = board.blockers(c) & ctx.pieceBB;
    if (pinnedPiece) moves &= BB::collinear(board.kingSq(c), ctx.square);

    return moves;
}

// minor piece eval: outposts + pawn shields
template <Color c, PieceType p>
inline Score Eval::evaluateMinorPiece(const PieceContext& ctx, U64 moves) {
    static_assert(p == PieceType::Knight || p == PieceType::Bishop);
    constexpr Color enemy = ~c;
    Score score;

    if (ctx.pieceBB & zones.outposts[c]) {
        if constexpr (p == PieceType::Knight)
            score += Conf::KnightOutpost;
        else
            score += Conf::BishopOutpost;
    } else if constexpr (p == PieceType::Knight) {
        if (moves & zones.outposts[c]) {
            score += Conf::ReachableOutpost;
        }
    }

    if (ctx.pieceBB & BB::pawnMoves<PawnMove::Push, enemy>(ctx.pawns)) {
        score += Conf::MinorPawnShield;
    }

    return score;
}

// bishop eval: long diagonals + pawn block penalty
template <Color c>
inline Score Eval::evaluateBishop(const PieceContext& ctx) {
    Score score;

    U64 xrayMoves       = BB::moves<PieceType::Bishop>(ctx.square, ctx.pawns);
    bool onLongDiagonal = BB::isMany(Conf::centerSquares & xrayMoves);
    if (onLongDiagonal) score += Conf::BishopLongDiagonal;

    score += bishopPawnBlockers<c>(ctx);

    return score;
}

// penalize bishops blocked by pawns on the same color squares
template <Color c>
inline Score Eval::bishopPawnBlockers(const PieceContext& ctx) const {
    constexpr Color enemy = ~c;

    bool isDarkSq      = ctx.pieceBB & Conf::darkSquares;
    U64 sameColorPawns = ctx.pawns & (isDarkSq ? Conf::darkSquares : Conf::lightSquares);
    int sameColorCount = BB::count(sameColorPawns);

    U64 blockedPawns       = ctx.pawns & BB::pawnMoves<PawnMove::Push, enemy>(ctx.occupied);
    U64 outsidePawnChain   = ctx.pieceBB & BB::pawnAttacks<c>(ctx.pawns);
    int pawnBlockingFactor = BB::count(blockedPawns & Conf::centerFiles) + !outsidePawnChain;

    return Conf::BishopBlockedByPawn * sameColorCount * pawnBlockingFactor;
}

/// rook eval: open/semi-open file bonus, closed+blocked penalty
template <Color c>
inline Score Eval::evaluateRook(const PieceContext& ctx) {
    constexpr Color enemy = ~c;

    U64 rookFileMask = BB::fileBB(fileOf(ctx.square));
    U64 filePawns    = ctx.pawns & rookFileMask;
    bool semiOpen    = !filePawns;

    if (semiOpen) {
        bool fullyOpen = !(ctx.enemyPawns & rookFileMask);
        return Conf::RookOpenFile[fullyOpen];
    } else if (filePawns & BB::pawnMoves<PawnMove::Push, enemy>(ctx.occupied)) {
        return Conf::RookClosedFile;
    }

    return ZERO_SCORE;
}

// queen eval: penalize discovered attacks
template <Color c>
inline Score Eval::evaluateQueen(const PieceContext& ctx) {
    if (discoveredAttack<c, PieceType::Bishop>(ctx) || discoveredAttack<c, PieceType::Rook>(ctx)) {
        return Conf::QueenDiscoveredAttack;
    }
    return ZERO_SCORE;
}

// penalize discovered attacks on the piece
template <Color c, PieceType p>
inline bool Eval::discoveredAttack(const PieceContext& ctx) {
    constexpr Color enemy = ~c;

    bool isDiscoverAttacked = false;
    U64 attackers           = board.pieces<p>(enemy) & BB::moves<p>(ctx.square, 0);

    BB::scan<c>(attackers, [&](Square pieceSq) {
        U64 piecesBetween  = BB::between(ctx.square, pieceSq) & ctx.occupied;
        isDiscoverAttacked = !BB::isMany(piecesBetween);
        if (isDiscoverAttacked) return;
    });

    return isDiscoverAttacked;
}

// king pawn-shelter score: friendly pawn shield vs enemy pawn storm
template <Color c>
Score Eval::kingShelter(Square kingSq) {
    constexpr Color enemy = ~c;
    Score score;

    File kingFile = fileOf(kingSq);
    Rank kingRank = rankOf(kingSq);

    U64 pawns      = board.pieces<PieceType::Pawn>(c);
    U64 enemyPawns = board.pieces<PieceType::Pawn>(enemy);

    U64 aheadOfKingMask = BB::spanFront<c>(BB::rankBB(kingRank));
    U64 pawnsAhead      = pawns & aheadOfKingMask & ~BB::pawnAttacks<enemy>(enemyPawns);
    U64 enemyPawnsAhead = enemyPawns & aheadOfKingMask;

    File file  = std::clamp(kingFile, File::F2, File::F7);
    score     += kingFileShelter<c>(pawnsAhead, enemyPawnsAhead, file - 1);
    score     += kingFileShelter<c>(pawnsAhead, enemyPawnsAhead, file);
    score     += kingFileShelter<c>(pawnsAhead, enemyPawnsAhead, file + 1);

    score += Conf::KingFile[idx(kingFile)];

    bool friendlyOpenFile  = !(pawns & BB::fileBB(kingFile));
    bool enemyOpenFile     = !(enemyPawns & BB::fileBB(kingFile));
    score                 += Conf::KingOpenFile[friendlyOpenFile][enemyOpenFile];

    return score;
}

// shelter score for one file: friendly pawn rank + enemy pawn rank
template <Color c>
inline Score Eval::kingFileShelter(U64 pawns, U64 enemyPawns, File file) {
    constexpr Color enemy = ~c;
    Score score;

    pawns      &= BB::fileBB(file);
    enemyPawns &= BB::fileBB(file);

    Square friendlyPawn = BB::selectSquare<enemy>(pawns);
    Square enemyPawn    = BB::selectSquare<enemy>(enemyPawns);
    Rank friendlyRank   = pawns ? relativeRank(friendlyPawn, c) : Rank::R1;
    Rank enemyRank      = enemyPawns ? relativeRank(enemyPawn, c) : Rank::R1;
    bool blocked        = pawns && (friendlyRank + 1 == enemyRank);

    score += Conf::PawnRankShelter[idx(friendlyRank)];
    score += Conf::PawnRankStorm[blocked][idx(enemyRank)];

    return score;
}

// convert raw danger into score {mg=quad scaling, eg=linear scaling}
template <Color c>
inline Score Eval::kingDanger(Square kingSq) {
    int danger  = kingDangerRaw<c>(kingSq);
    int mgScore = danger * danger / 2048;
    int egScore = danger / 8;
    return {mgScore, egScore};
}

// return 'raw' king danger metric (checks, weak defenses, pins, etc)
template <Color c>
int Eval::kingDangerRaw(Square kingSq) {
    constexpr Color enemy = ~c;
    int danger            = 0;

    U64 defended       = attacks.by[c][PieceIdx::All];
    U64 attacked       = attacks.by[enemy][PieceIdx::All];
    U64 kqDefense      = attacks.by[c][PieceIdx::Queen] | attacks.by[c][PieceIdx::King];
    U64 weaklyDefended = (~defended | kqDefense) & attacked & ~attacks.byTwo[c];
    U64 safelyAttacked = ~board.pieces<PieceType::All>(enemy) &
                         (~defended | (weaklyDefended & attacks.byTwo[enemy]));

    U64 knightMoves       = attacks.by[enemy][PieceIdx::Knight];
    U64 knightChecks      = BB::moves<PieceType::Knight>(kingSq) & knightMoves;
    U64 safeKnightChecks  = knightChecks & safelyAttacked;
    danger               += kingChecksDanger<PieceType::Knight>(safeKnightChecks, knightChecks);

    U64 occupancy      = board.occupancy();
    U64 straightChecks = BB::moves<PieceType::Rook>(kingSq, occupancy);
    U64 diagonalChecks = BB::moves<PieceType::Bishop>(kingSq, occupancy);

    U64 rookChecks      = straightChecks & attacks.by[enemy][PieceIdx::Rook];
    U64 safeRookChecks  = rookChecks & safelyAttacked;
    danger             += kingChecksDanger<PieceType::Rook>(safeRookChecks, rookChecks);

    U64 queenChecks      = (straightChecks | diagonalChecks) & attacks.by[enemy][PieceIdx::Queen];
    U64 badQueenChecks   = rookChecks | attacks.by[c][PieceIdx::Queen];
    U64 safeQueenChecks  = queenChecks & safelyAttacked & ~badQueenChecks;
    danger              += kingChecksDanger<PieceType::Queen>(safeQueenChecks, queenChecks);

    U64 bishopChecks      = diagonalChecks & attacks.by[enemy][PieceIdx::Bishop];
    U64 safeBishopChecks  = bishopChecks & safelyAttacked & ~queenChecks;
    danger               += kingChecksDanger<PieceType::Bishop>(safeBishopChecks, bishopChecks);

    int kingZoneAttacks  = kingAttackers.value[c] * kingAttackers.count[c];
    danger              += kingZoneAttacks;

    int kingZoneWeakness  = BB::count(zones.king[c] & weaklyDefended);
    danger               += Conf::WeakKingZoneDanger * kingZoneWeakness;

    int pinsCount  = BB::count(board.blockers(c));
    danger        += Conf::PinnedPieceDanger * pinsCount;

    return danger;
}

// scale danger non-linearly, 1 check = 1×, 2 checks = ~1.3×, 3 checks = 1.5×, asymptotes to 2×
template <PieceType p>
inline int Eval::kingChecksDanger(U64 safeChecks, U64 allChecks) {
    constexpr auto pt = idx(p);
    int checkCount    = BB::count(safeChecks ? safeChecks : allChecks);
    auto checkValue   = safeChecks ? Conf::SafeCheckDanger : Conf::UnsafeCheckDanger;
    return (checkValue[pt] * checkCount * 2) / (checkCount + 1);
}

// scale endgame eval toward 0 in draw-ish pawn endings,
inline float Eval::scaleFactor() const {
    int pawnCount = board.count(board.sideToMove(), PieceType::Pawn);
    return std::min(SCALE_LIMIT, 36 + 5 * pawnCount) / float(SCALE_LIMIT);
}

// blend midgame / endgame scores based on game phase
inline int Eval::taperScore(Score score) const {
    int mgPhase = phase();
    int egPhase = PHASE_LIMIT - mgPhase;
    return ((score.mg * mgPhase) + (score.eg * egPhase)) / PHASE_LIMIT;
}

// game phase: 0 = endgame, PHASE_LIMIT = middlegame
inline int Eval::phase() const {
    int npm      = board.nonPawnMaterial(WHITE) + board.nonPawnMaterial(BLACK);
    int material = std::clamp(npm, EG_LIMIT, MG_LIMIT);
    return ((material - EG_LIMIT) * PHASE_LIMIT) / (MG_LIMIT - EG_LIMIT);
}

inline int eval(const Board& board) { return Eval(board).evaluate(); }

// eval + per-term score breakdown (for logging/debug)
class EvalVerbose {
   private:
    Eval eval;
    ScoreTracker scoreTracker;

   public:
    EvalVerbose(const Board& b)
        : eval(b, [this](EvalTerm term, Color c, Score score) {
              scoreTracker.addScore(term, score, c);
          }) {}

    int evaluate() { return eval.evaluate(); };

    std::string toString() const;
    friend std::ostream& operator<<(std::ostream& os, const EvalVerbose& eval);
};
