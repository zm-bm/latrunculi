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

/**
 * @brief Evaluates a chess position.
 *
 * This class provides methods to evaluate various aspects of a chess position,
 * including material balance, piece activity, king safety, and more.
 */
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

    //---------------------------------------
    // Evaluation data
    //---------------------------------------

    struct AttackData {
        U64 by[N_COLORS][N_PIECES] = {{0}};
        U64 byTwo[N_COLORS]        = {0};
    } attacks;

    struct ZoneData {
        U64 outposts[N_COLORS] = {0};
        U64 mobility[N_COLORS] = {0};
        U64 king[N_COLORS]     = {0};
    } zones;

    struct EnemyKingAttacksData {
        int count[N_COLORS] = {0};
        int value[N_COLORS] = {0};
    } kingAttackers;

    struct ScoreData {
        Score mobility[N_COLORS] = {ZERO_SCORE};
        Score threats[N_COLORS]  = {ZERO_SCORE};
        Score finalScore         = ZERO_SCORE;
        int finalResult          = 0;
    } scores;

    //---------------------------------------
    // Initialization methods
    //---------------------------------------

    template <Color>
    void initialize();

    template <Color c>
    U64 calculateOutpostZone();

    template <Color c>
    U64 calculateMobilityZone();

    template <Color c>
    U64 calculateKingZone();

    //---------------------------------------
    // Core evaluation methods
    //---------------------------------------

    template <EvalTerm, Color = WHITE>
    Score evaluateTerm();

    template <Color>
    Score evaluatePawns();

    template <Color, PieceType>
    Score evaluatePieces();

    template <Color>
    Score evaluateKing();

    //---------------------------------------
    // Update evaluation data methods
    //---------------------------------------

    template <Color c, PieceType p>
    void updatePieceAttacks(U64 moves);

    template <Color c, PieceType p>
    void updatePieceMobility(U64 moves);

    template <Color c, PieceType p>
    void updatePieceThreats(const PieceContext& ctx);

    template <Color c, PieceType p>
    Score updatePieceKingAttackers(const PieceContext& ctx, U64 moves);

    //---------------------------------------
    // Piece evaluation methods
    //---------------------------------------

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

    //---------------------------------------
    // King evaluation methods
    //---------------------------------------

    template <Color>
    Score kingShelter(Square kingSq);

    template <Color>
    Score kingFileShelter(U64 pawns, U64 enemyPawns, File file);

    template <Color>
    Score kingDanger(Square kingSq);

    template <PieceType>
    int kingChecksDanger(U64 safeChecks, U64 allChecks);

    //---------------------------------------
    // Helper methods
    //---------------------------------------

    float scaleFactor() const;
    int taperScore(Score score) const;
    int phase() const;
    int nonPawnMaterial(Color) const;

    friend class EvalTest;
};

/**
 * @brief Constructs an Eval object with a given board and optional evaluation term callback.
 * @param b The board to evaluate.
 * @param termCallback Optional callback to be called for each evaluation term.
 */
inline Eval::Eval(const Board& b, TermCallback termCallback)
    : board{b}, termCallback{termCallback} {
    initialize<WHITE>();
    initialize<BLACK>();
}

/**
 * @brief Main function for evaluating the board position.
 * @return int The evaluation of the position.
 */
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
    // complex terms
    score += evaluateTerm<EvalTerm::King, WHITE>() - evaluateTerm<EvalTerm::King, BLACK>();
    score += evaluateTerm<EvalTerm::Mobility, WHITE>() - evaluateTerm<EvalTerm::Mobility, BLACK>();
    score += evaluateTerm<EvalTerm::Threats, WHITE>() - evaluateTerm<EvalTerm::Threats, BLACK>();

    // scale endgame in drawn positions
    score.eg *= scaleFactor();

    // blend midgame and endgame scores into a final evaluation
    int result = taperScore(score);

    // final result relative to side to move and add tempo bonus
    if (board.sideToMove() == BLACK) result = -result;
    result += TEMPO_BONUS;

    scores.finalScore  = score;
    scores.finalResult = result;
    return result;
}

/**
 * @brief Initializes evaluation data for a specific color
 * @tparam c Color for which to initialize data
 */
template <Color c>
inline void Eval::initialize() {
    zones.outposts[c] = calculateOutpostZone<c>();
    zones.mobility[c] = calculateMobilityZone<c>();
    zones.king[c]     = calculateKingZone<c>();
}

/**
 * @brief Calculates squares where pieces can be safely posted behind pawn protection
 * @tparam c Color for which to calculate outposts
 * @return Bitboard of potential outpost squares
 */
template <Color c>
inline U64 Eval::calculateOutpostZone() {
    constexpr Color enemy     = ~c;
    constexpr U64 outpostMask = (c == WHITE) ? WHITE_OUTPOSTS : BLACK_OUTPOSTS;

    U64 pawns      = board.pieces<PieceType::Pawn>(c);
    U64 enemyPawns = board.pieces<PieceType::Pawn>(enemy);

    // Potential outposts are squares where:
    // 1. Cannot be attacked by enemy pawns
    // 2. Supported by friendly pawns
    // 3. On file 4-6 relative to side
    return (~BB::pawnAttackSpan<enemy>(enemyPawns) & BB::pawnAttacks<c>(pawns) & outpostMask);
}

/**
 * @brief Calculates squares where pieces can safely move (not attacked by enemy pawns)
 *
 * Mobility zone is defined as:
 * 1. Not attacked by enemy pawns
 * 2. Not occupied by the king
 * 3. Not occupied by friendly pawns on the second rank
 *
 * @tparam c Color for which to calculate mobility zone
 * @return Bitboard of squares for mobility evaluation
 */
template <Color c>
inline U64 Eval::calculateMobilityZone() {
    constexpr Color enemy = ~c;
    constexpr U64 rank2   = (c == WHITE) ? BB::rankBB(Rank::R2) : BB::rankBB(Rank::R7);

    Square kingSq  = board.kingSq(c);
    U64 pawns      = board.pieces<PieceType::Pawn>(c);
    U64 enemyPawns = board.pieces<PieceType::Pawn>(enemy);

    return ~(BB::pawnAttacks<enemy>(enemyPawns) | BB::set(kingSq) | (pawns & rank2));
}

/**
 * @brief Calculates 3x3 area around the king for king safety evaluation
 * @tparam c Color for which to calculate king zone
 * @return Bitboard representing the king's danger zone
 */
template <Color c>
inline U64 Eval::calculateKingZone() {
    Square kingSq = board.kingSq(c);
    Square center = makeSquare(std::clamp(fileOf(kingSq), File::F2, File::F7),
                               std::clamp(rankOf(kingSq), Rank::R2, Rank::R7));

    // King zone is a 3x3 square area around the king
    return BB::moves<PieceType::King>(center) | BB::set(center);
}

/**
 * @brief Evaluates a specific term in the evaluation function
 * @tparam term Evaluation term to calculate
 * @tparam c Color for which to evaluate the term
 * @return Score for the specified term
 * @note Calls termCallback if registered
 */
template <EvalTerm term, Color c>
Score Eval::evaluateTerm() {
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

/**
 * @brief Evaluates pawn structure (isolated, backward, doubled pawns)
 * @tparam c Color for which to evaluate pawns
 * @return Score representing pawn structure quality
 * @note Updates attack tables as a side effect
 */
template <Color c>
Score Eval::evaluatePawns() {
    constexpr Color enemy = ~c;
    Score score;

    U64 pawns             = board.pieces<PieceType::Pawn>(c);
    U64 pawnAttacks       = BB::pawnAttacks<c>(pawns);
    U64 pawnDoubleAttacks = BB::pawnDoubleAttacks<c>(pawns);
    U64 enemyPawns        = board.pieces<PieceType::Pawn>(enemy);

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

/**
 * @brief Evaluates all pieces of a specific type
 * @tparam c Color of the pieces
 * @tparam p Type of pieces to evaluate
 * @return Combined score for all pieces of the specified type
 * @note Updates attack tables and other evaluation data
 */
template <Color c, PieceType p>
Score Eval::evaluatePieces() {
    constexpr Color enemy = ~c;
    Score score;

    U64 occupied   = board.occupancy();
    U64 pawns      = board.pieces<PieceType::Pawn>(c);
    U64 enemyPawns = board.pieces<PieceType::Pawn>(enemy);

    // Process each piece
    U64 pieceBB = board.pieces<p>(c);
    BB::scan<c>(pieceBB, [&](Square sq) {
        PieceContext context{.square     = sq,
                             .pieceBB    = BB::set(sq),
                             .occupied   = occupied,
                             .pawns      = pawns,
                             .enemyPawns = enemyPawns};

        U64 moves = calculateMoves<c, p>(context);
        updatePieceAttacks<c, p>(moves);
        updatePieceMobility<c, p>(moves);
        updatePieceThreats<c, p>(context);
        score += updatePieceKingAttackers<c, p>(context, moves);

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

    // Special case for bishop pair
    if constexpr (p == PieceType::Bishop) {
        if (board.count(c, PieceType::Bishop) > 1) score += Conf::BishopPair;
    }

    return score;
}

/**
 * @brief Evaluates king safety based on pawn shelter and enemy attacks
 * @tparam c Color of the king to evaluate
 * @return Score representing king safety
 */
template <Color c>
Score Eval::evaluateKing() {
    constexpr Color enemy = ~c;

    Square kingSq   = board.kingSq(c);
    U64 kingAttacks = BB::moves<PieceType::King>(kingSq);
    updatePieceAttacks<c, PieceType::King>(kingAttacks);

    Score shelter    = kingShelter<c>(kingSq);
    Score shelterOO  = board.canCastleOO(c) ? kingShelter<c>(KingDestinationOO[c]) : ZERO_SCORE;
    Score shelterOOO = board.canCastleOOO(c) ? kingShelter<c>(KingDestinationOOO[c]) : ZERO_SCORE;

    Score score  = std::max({shelter, shelterOO, shelterOOO});
    score       -= kingDanger<c>(kingSq);

    return score;
}

/**
 * @brief Updates attack bitboards for a piece type
 * @tparam c Color of the piece
 * @tparam p Type of the piece
 * @param moves Bitboard of possible moves/attacks
 */
template <Color c, PieceType p>
inline void Eval::updatePieceAttacks(U64 moves) {
    attacks.byTwo[c]             |= (attacks.by[c][PieceIdx::All] & moves);
    attacks.by[c][PieceIdx::All] |= moves;
    attacks.by[c][idx(p)]        |= moves;
}

/**
 * @brief Calculates mobility score for a piece type
 * @tparam c Color of the piece
 * @tparam p Type of the piece
 * @param moves Bitboard of possible moves
 */
template <Color c, PieceType p>
inline void Eval::updatePieceMobility(U64 moves) {
    U64 moveCount       = BB::count(moves & zones.mobility[c]);
    scores.mobility[c] += Conf::Mobility[idx(p)][moveCount];
}

/**
 * @brief Evaluates threats against a piece (when attackers outnumber defenders)
 * @tparam c Color of the piece
 * @tparam p Type of the piece
 * @param ctx Context information for the piece
 */
template <Color c, PieceType p>
inline void Eval::updatePieceThreats(const PieceContext& ctx) {
    constexpr Color enemy = ~c;

    // Check if attackers outnumber defenders
    U64 defenders = board.attacksTo(ctx.square, c);
    U64 attackers = board.attacksTo(ctx.square, enemy);
    if (BB::count(attackers) > BB::count(defenders)) {
        scores.threats[c] += Conf::WeakPiece[idx(p)];
    }
}

/**
 * @brief Evaluates pieces attacking the enemy king zone
 * @tparam c Color of the piece
 * @tparam p Type of the piece
 * @param ctx Context information for the piece
 * @param moves Bitboard of possible moves
 * @return Score bonus for king attacks
 */
template <Color c, PieceType p>
inline Score Eval::updatePieceKingAttackers(const PieceContext& ctx, U64 moves) {
    constexpr Color enemy = ~c;

    // Check if the piece is attacking the enemy king zone
    if (moves & zones.king[enemy]) {
        kingAttackers.count[c]++;
        kingAttackers.value[c] += Conf::PieceKingAttacks[idx(p)];
    }
    // Check for x-ray attacks on enemy king zone
    else if ((p == PieceType::Bishop && (BB::moves<p>(ctx.square, ctx.pawns) & zones.king[enemy])) |
             (p == PieceType::Knight && (BB::moves<p>(ctx.square) & zones.king[enemy]))) {
        return Conf::KingZoneXrayAttack;
    }

    return ZERO_SCORE;
}

/**
 * @brief Calculates legal moves for a piece, considering pins
 * @tparam c Color of the piece
 * @tparam p Type of the piece
 * @param ctx Context information for the piece
 * @return Bitboard of legal moves
 */
template <Color c, PieceType p>
inline U64 Eval::calculateMoves(const PieceContext& ctx) {
    U64 moves = BB::moves<p>(ctx.square, ctx.occupied);

    // If piece is pinned, restrict moves to the line of attack
    if (board.blockers(c) & ctx.pieceBB) {
        moves &= BB::collinear(board.kingSq(c), ctx.square);
    }

    return moves;
}

/**
 * @brief Evaluates minor pieces (knights and bishops) for outposts and pawn shields
 * @tparam c Color of the piece
 * @tparam p Type of the piece (Knight or Bishop)
 * @param ctx Context information for the piece
 * @param moves Bitboard of legal moves
 * @return Score bonus for the minor piece position
 */
template <Color c, PieceType p>
inline Score Eval::evaluateMinorPiece(const PieceContext& ctx, U64 moves) {
    constexpr Color enemy = ~c;
    Score score;

    // Outpost bonus
    if (ctx.pieceBB & zones.outposts[c]) {
        if constexpr (p == PieceType::Knight)
            score += Conf::KnightOutpost;
        else
            score += Conf::BishopOutpost;
    }
    // Reachable outpost for knights
    else if constexpr (p == PieceType::Knight) {
        if (moves & zones.outposts[c]) {
            score += Conf::ReachableOutpost;
        }
    }

    // Pawn shield bonus
    if (ctx.pieceBB & BB::pawnMoves<PawnMove::Push, enemy>(ctx.pawns)) {
        score += Conf::MinorPawnShield;
    }

    return score;
}

/**
 * @brief Evaluates bishop-specific positional factors
 * @tparam c Color of the bishop
 * @param ctx Context information for the bishop
 * @return Score representing bishop position quality
 */
template <Color c>
inline Score Eval::evaluateBishop(const PieceContext& ctx) {
    Score score;

    // Bonus for bishops on long diagonals
    if (BB::isMany(CENTER_SQUARES & BB::moves<PieceType::Bishop>(ctx.square, ctx.pawns))) {
        score += Conf::BishopLongDiagonal;
    }

    // Penalty for pawns blocking the bishop's path
    score += bishopPawnBlockers<c>(ctx);

    return score;
}

/**
 * @brief Calculates penalty for pawns blocking a bishop's movement
 * @tparam c Color of the bishop
 * @param ctx Context information for the bishop
 * @return Negative score for blocked bishop
 */
template <Color c>
inline Score Eval::bishopPawnBlockers(const PieceContext& ctx) const {
    constexpr Color enemy = ~c;

    U64 sameColorPawns = ctx.pawns & (ctx.pieceBB & DARK_SQUARES ? DARK_SQUARES : LIGHT_SQUARES);
    int pawnCount      = BB::count(sameColorPawns);

    U64 blockedPawns       = ctx.pawns & BB::pawnMoves<PawnMove::Push, enemy>(ctx.occupied);
    U64 bishopOutsideChain = ctx.pieceBB & BB::pawnAttacks<c>(ctx.pawns);
    int pawnBlockingFactor = BB::count(blockedPawns & CENTER_FILES) + !bishopOutsideChain;

    return Conf::BishopBlockedByPawn * pawnCount * pawnBlockingFactor;
}

/**
 * @brief Evaluates rook-specific positional factors (open files)
 * @tparam c Color of the rook
 * @param ctx Context information for the rook
 * @return Score representing rook position quality
 */
template <Color c>
inline Score Eval::evaluateRook(const PieceContext& ctx) {
    constexpr Color enemy = ~c;
    U64 rookFileMask      = BB::fileBB(fileOf(ctx.square));

    // Open file bonus
    if (!(ctx.pawns & rookFileMask)) {
        return Conf::RookOpenFile[!(ctx.enemyPawns & rookFileMask)];
    }
    // Closed file penalty
    else if (ctx.pawns & rookFileMask & BB::pawnMoves<PawnMove::Push, enemy>(ctx.occupied)) {
        return Conf::RookClosedFile;
    }

    return ZERO_SCORE;
}

/**
 * @brief Evaluates queen-specific positional factors
 * @tparam c Color of the queen
 * @param ctx Context information for the queen
 * @return Score representing queen position quality
 */
template <Color c>
inline Score Eval::evaluateQueen(const PieceContext& ctx) {
    // Check for discovered attacks on a queen by enemy bishops or rooks
    if (discoveredAttack<c, PieceType::Bishop>(ctx) || discoveredAttack<c, PieceType::Rook>(ctx)) {
        return Conf::QueenDiscoveredAttack;
    }
    return ZERO_SCORE;
}

/**
 * @brief Detects if a piece is vulnerable to discovered attacks
 * @tparam c Color of the piece
 * @tparam p Type of attacking piece to check for
 * @param ctx Context information for the piece
 * @return True if piece is vulnerable to discovered attack
 */
template <Color c, PieceType p>
inline bool Eval::discoveredAttack(const PieceContext& ctx) {
    constexpr Color enemy   = ~c;
    bool isDiscoverAttacked = false;

    // Process enemy pieces that can attack the square
    U64 attackers = board.pieces<p>(enemy) & BB::moves<p>(ctx.square, 0);
    BB::scan<c>(attackers, [&](Square pieceSq) {
        U64 piecesBetween = BB::between(ctx.square, pieceSq) & ctx.occupied;

        // Check if there is only one piece between the attacker and the square
        if (!BB::isMany(piecesBetween)) {
            isDiscoverAttacked = true;
            return;
        };
    });

    return isDiscoverAttacked;
}

/**
 * @brief Evaluates pawn shelter for king safety
 * @tparam c Color of the king
 * @param kingSq Square of the king
 * @return Score representing pawn shelter quality
 */
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

/**
 * @brief Evaluates pawn shelter on a specific file
 * @tparam c Color of the king
 * @param pawns Bitboard of friendly pawns
 * @param enemyPawns Bitboard of enemy pawns
 * @param file File to evaluate
 * @return Score for pawn shelter on this file
 */
template <Color c>
inline Score Eval::kingFileShelter(U64 pawns, U64 enemyPawns, File file) {
    constexpr Color enemy = ~c;
    Score score;

    pawns      &= BB::fileBB(file);
    enemyPawns &= BB::fileBB(file);

    Square ourPawnSquare   = BB::selectSquare<enemy>(pawns);
    Square enemyPawnSquare = BB::selectSquare<enemy>(enemyPawns);

    Rank ourPawnRank   = pawns ? relativeRank(ourPawnSquare, c) : Rank::R1;
    Rank enemyPawnRank = enemyPawns ? relativeRank(enemyPawnSquare, c) : Rank::R1;
    bool isPawnBlocked = (ourPawnRank != Rank::R1) && ((ourPawnRank + 1) == enemyPawnRank);

    score += Conf::PawnRankShelter[idx(ourPawnRank)];
    score += Conf::PawnRankStorm[isPawnBlocked][idx(enemyPawnRank)];

    return score;
}

/**
 * @brief Calculates danger to the king from enemy attacks
 * @tparam c Color of the king
 * @param kingSq Square of the king
 * @return Negative score proportional to king danger
 */
template <Color c>
Score Eval::kingDanger(Square kingSq) {
    constexpr Color enemy = ~c;
    int danger            = 0;

    // calculate safe potential checks on our king
    U64 kqOnlyDefense = ~attacks.by[c][PieceIdx::All] | attacks.by[c][PieceIdx::King] |
                        attacks.by[c][PieceIdx::Queen];
    U64 weaklyDefended  = kqOnlyDefense & attacks.by[enemy][PieceIdx::All] & ~attacks.byTwo[c];
    U64 safeChecks      = ~attacks.by[c][PieceIdx::All] | (weaklyDefended & attacks.byTwo[enemy]);
    safeChecks         &= ~board.pieces<PieceType::All>(enemy);

    // calculate knight checks danger
    U64 knightChecks      = BB::moves<PieceType::Knight>(kingSq) & attacks.by[c][PieceIdx::Knight];
    U64 safeKnightChecks  = knightChecks & safeChecks;
    danger               += kingChecksDanger<PieceType::Knight>(safeKnightChecks, knightChecks);

    // calculate sliding checks
    U64 straightChecks = BB::moves<PieceType::Rook>(kingSq, board.pieces<PieceType::All>());
    U64 diagonalChecks = BB::moves<PieceType::Bishop>(kingSq, board.pieces<PieceType::All>());

    // calculate rook checks danger
    U64 rookChecks      = straightChecks & attacks.by[enemy][PieceIdx::Rook];
    U64 safeRookChecks  = rookChecks & safeChecks;
    danger             += kingChecksDanger<PieceType::Rook>(safeRookChecks, rookChecks);

    // calculate queen checks danger (removing available rook checks)
    U64 queenChecks     = (straightChecks | diagonalChecks) & attacks.by[enemy][PieceIdx::Queen];
    U64 safeQueenChecks = queenChecks & safeChecks & ~(rookChecks | attacks.by[c][PieceIdx::Queen]);
    danger += kingChecksDanger<PieceType::Queen>(safeQueenChecks, queenChecks);

    // calculate bishop checks danger (removing available queen checks)
    U64 bishopChecks      = diagonalChecks & attacks.by[enemy][PieceIdx::Bishop];
    U64 safeBishopChecks  = bishopChecks & safeChecks & ~queenChecks;
    danger               += kingChecksDanger<PieceType::Bishop>(safeBishopChecks, bishopChecks);

    // king zone attacks
    danger += kingAttackers.value[enemy] * kingAttackers.count[enemy];

    // king zone weaknesses
    danger += 150 * BB::count(zones.king[c] & weaklyDefended);

    // pinned pieces
    danger += 50 * BB::count(board.blockers(c));

    return {danger * danger / 2048,  // scale midgame danger quadratically
            danger / 8};             // scale endgame danger linearly
}

/**
 * @brief Calculates danger from potential checks against the king
 * @tparam p Type of piece delivering the check
 * @param safeChecks Bitboard of safe checking squares
 * @param allChecks Bitboard of all possible checking squares
 * @return Danger value from the potential checks
 */
template <PieceType p>
inline int Eval::kingChecksDanger(U64 safeChecks, U64 allChecks) {
    constexpr auto pt = idx(p);
    int checkCount    = safeChecks ? BB::count(safeChecks) : BB::count(allChecks);
    auto checkValue   = safeChecks ? Conf::PieceSafeCheck : Conf::PieceUnsafeCheck;

    // non-linear scaling: 1 check = 1×, 2 = ~1.3×, 3 = 1.5×, asymptotes to 2×
    return (checkValue[pt] * checkCount * 2) / (checkCount + 1);
}

/**
 * @brief Calculates endgame scaling factor based on pawn count
 * @return Factor between 0-1 to scale endgame evaluation
 */
inline float Eval::scaleFactor() const {
    // scale proportionally with pawns
    int pawnCount = board.count(board.sideToMove(), PieceType::Pawn);
    return std::min(SCALE_LIMIT, 36 + 5 * pawnCount) / float(SCALE_LIMIT);
}

/**
 * @brief Interpolates between middlegame and endgame scores based on game phase
 * @param score Score to interpolate
 * @return Final tapered score
 */
inline int Eval::taperScore(Score score) const {
    int mgPhase = phase();
    int egPhase = PHASE_LIMIT - mgPhase;
    return ((score.mg * mgPhase) + (score.eg * egPhase)) / PHASE_LIMIT;
}

/**
 * @brief Determines current game phase based on remaining material
 * @return Phase value (higher = more middlegame-like)
 */
inline int Eval::phase() const {
    int npm      = nonPawnMaterial(WHITE) + nonPawnMaterial(BLACK);
    int material = std::clamp(npm, EG_LIMIT, MG_LIMIT);
    return ((material - EG_LIMIT) * PHASE_LIMIT) / (MG_LIMIT - EG_LIMIT);
}

/**
 * @brief Calculates total material value excluding pawns
 * @param c Color for which to calculate material
 * @return Total non-pawn material value
 */
inline int Eval::nonPawnMaterial(Color c) const {
    return ((board.count(c, PieceType::Knight) * KNIGHT_VALUE_MG) +
            (board.count(c, PieceType::Bishop) * BISHOP_VALUE_MG) +
            (board.count(c, PieceType::Rook) * ROOK_VALUE_MG) +
            (board.count(c, PieceType::Queen) * QUEEN_VALUE_MG));
}

/**
 * @brief Helper function to evaluate a position
 * @param board Position to evaluate
 * @return Evaluation score from current player's perspective
 */
inline int eval(const Board& board) { return Eval(board).evaluate(); }

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
