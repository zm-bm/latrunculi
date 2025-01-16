#include "eval.hpp"

#include <gtest/gtest.h>

#include "bb.hpp"
#include "board.hpp"
#include "chess.hpp"
#include "constants.hpp"

TEST(EvalTest, ValuesMirrored) {
    for (int ph = MIDGAME; ph < N_PHASES; ++ph) {
        for (int pt = PAWN; pt < N_PIECES; ++pt) {
            EXPECT_EQ(Eval::pieceValue(Phase(ph), WHITE, PieceType(pt)),
                      -Eval::pieceValue(Phase(ph), BLACK, PieceType(pt)));
        }
    }
}

TEST(Eval_psqValue, ValuesMirror) {
    for (int ph = MIDGAME; ph < N_PHASES; ++ph) {
        for (int pt = PAWN; pt < N_PIECES; ++pt) {
            for (int sq = A1; sq < N_SQUARES; ++sq) {
                int bsq = N_SQUARES - 1 - sq;
                EXPECT_EQ(Eval::psqValue(Phase(ph), WHITE, PieceType(pt), Square(sq)),
                          -Eval::psqValue(Phase(ph), BLACK, PieceType(pt), Square(bsq)));
            }
        }
    }
}

TEST(Eval_tempoBonus, CorrectSignage) {
    EXPECT_EQ(Eval::tempoBonus(WHITE), TEMPO_BONUS);
    EXPECT_EQ(Eval::tempoBonus(BLACK), -TEMPO_BONUS);
}

TEST(Eval_calculatePhase, LimitsAreDefault) {
    EXPECT_EQ(Eval::calculatePhase(0), 0);
    EXPECT_EQ(Eval::calculatePhase(EG_LIMIT), 0);
    EXPECT_EQ(Eval::calculatePhase(MG_LIMIT), PHASE_LIMIT);
}

TEST(Eval_calculatePhase, CorrectInterpolation) {
    Chess c = Chess(STARTFEN);
    EXPECT_EQ(Eval::calculatePhase(c.nonPawnMaterial(BLACK)), 49);
    EXPECT_EQ(Eval::calculatePhase(c.nonPawnMaterial(BLACK) * 2), PHASE_LIMIT);
}

TEST(Eval_isolatedPawns, StartPositionNoIsolatedPawns) {
    Board b(STARTFEN);
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(WHITE)), 0);
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(BLACK)), 0);
}

TEST(Eval_isolatedPawns, IsolatedPawnsOnA2AndG7) {
    Board b("rnbqkbnr/ppppp1p1/8/8/8/8/P1PPPPPP/RNBQKBNR w KQkq - 0 1");
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(WHITE)), BB::set(A2));
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(BLACK)), BB::set(G7));
}

TEST(Eval_isolatedPawns, IsolatedPawnsIncludesAllPawnsOnFile) {
    Board b("k7/p7/8/P7/8/P7/P7/K7 w KQkq - 0 1");
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(WHITE)),
              BB::set(A2) | BB::set(A3) | BB::set(A5));
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(BLACK)), BB::set(A7));
}

TEST(Eval_backwardsPawns, StartPositionNoBackwardsPawns) {
    Board b(STARTFEN);
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::backwardsPawns<WHITE>(wPawns, bPawns), 0);
    EXPECT_EQ(Eval::backwardsPawns<BLACK>(bPawns, wPawns), 0);
}

TEST(Eval_backwardsPawns, BackwardsPawns) {
    Board b("4k3/2p5/1p6/1P6/P7/8/8/4K3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::backwardsPawns<WHITE>(wPawns, bPawns), BB::set(A4));
    EXPECT_EQ(Eval::backwardsPawns<BLACK>(bPawns, wPawns), BB::set(C7));
}

TEST(Eval_doubledPawns, StartPositionNoDoubledPawns) {
    Board b(STARTFEN);
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);

    EXPECT_EQ(Eval::doubledPawns<WHITE>(wPawns), 0);
    EXPECT_EQ(Eval::doubledPawns<BLACK>(bPawns), 0);
}

TEST(Eval_doubledPawns, WhiteDoubledPawnOnA4) {
    Board b("4k3/8/pp6/p7/P7/8/P7/4K3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::doubledPawns<WHITE>(wPawns), BB::set(A4));
    EXPECT_EQ(Eval::doubledPawns<BLACK>(bPawns), 0);
}

TEST(Eval_blockedPawns, Blocked) {
    Board b("4k3/8/8/p7/P7/8/8/4K3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::blockedPawns<WHITE>(wPawns, bPawns), BB::set(A4));
    EXPECT_EQ(Eval::blockedPawns<BLACK>(bPawns, wPawns), BB::set(A5));
}

TEST(Eval_blockedPawns, NotBlocked) {
    Board b("4k3/8/8/p7/8/P7/8/4K3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::blockedPawns<WHITE>(wPawns, bPawns), 0);
    EXPECT_EQ(Eval::blockedPawns<BLACK>(bPawns, wPawns), 0);
}

TEST(Eval_oppositeBishops, EmptyBoardNoOppositeBishops) {
    Board b(EMPTYFEN);
    EXPECT_FALSE(Eval::oppositeBishops(b.getPieces<BISHOP>(WHITE), b.getPieces<BISHOP>(BLACK)));
}

TEST(Eval_oppositeBishops, StartPositionNoOppositeBishops) {
    Board b(STARTFEN);
    EXPECT_TRUE(Eval::oppositeBishops(b.getPieces<BISHOP>(WHITE), b.getPieces<BISHOP>(BLACK)));
}

TEST(Eval_oppositeBishops, SameColorBishops) {
    Board b("3bk3/8/8/8/8/8/8/2B1K3 w - - 0 1");
    EXPECT_FALSE(Eval::oppositeBishops(b.getPieces<BISHOP>(WHITE), b.getPieces<BISHOP>(BLACK)));
}

TEST(Eval_oppositeBishops, OppositeColorBishops) {
    Board b("3bk3/8/8/8/8/8/8/3BK3 w - - 0 1");
    EXPECT_TRUE(Eval::oppositeBishops(b.getPieces<BISHOP>(WHITE), b.getPieces<BISHOP>(BLACK)));
}

TEST(Eval_outpostSquare, EmptyPosition) {
    Board b(EMPTYFEN);
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::outpostSquares<WHITE>(wPawns, bPawns), 0);
    EXPECT_EQ(Eval::outpostSquares<BLACK>(bPawns, wPawns), 0);
}

TEST(Eval_outpostSquare, StartPosition) {
    Board b(STARTFEN);
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::outpostSquares<WHITE>(wPawns, bPawns), 0);
    EXPECT_EQ(Eval::outpostSquares<BLACK>(bPawns, wPawns), 0);
}

TEST(Eval_outpostSquare, WhiteOutpostOnD5) {
    Board b("r4rk1/pp3ppp/3p2n1/2p5/4P3/2N5/PPP2PPP/2KRR3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::outpostSquares<WHITE>(wPawns, bPawns), BB::set(D5));
    EXPECT_EQ(Eval::outpostSquares<BLACK>(bPawns, wPawns), 0);
}

TEST(Eval_outpostSquare, BlackOutpostOnD4) {
    Board b("r4rk1/pp2pppp/3pn3/2p5/2P1P3/1N6/PP3PPP/2KRR3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::outpostSquares<WHITE>(wPawns, bPawns), 0);
    EXPECT_EQ(Eval::outpostSquares<BLACK>(bPawns, wPawns), BB::set(D4));
}

TEST(Eval_outpostSquare, NoOupostOn7thRank) {
    Board b("r4rk1/1p2pppp/1P1pn3/2p5/8/pNPPP3/P4PPP/2KRR3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::outpostSquares<WHITE>(wPawns, bPawns), 0);
    EXPECT_EQ(Eval::outpostSquares<BLACK>(bPawns, wPawns), 0);
}
