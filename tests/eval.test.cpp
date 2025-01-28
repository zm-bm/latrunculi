#include "eval.hpp"

#include <gtest/gtest.h>

#include "bb.hpp"
#include "board.hpp"
#include "chess.hpp"
#include "constants.hpp"

// --- Tests for piece / square values ---
TEST(EvalTest_pieceValue, WhitePiecesMirrorBlackPieces) {
    for (int ph = MIDGAME; ph < N_PHASES; ++ph) {
        for (int pt = PAWN; pt < N_PIECES; ++pt) {
            EXPECT_EQ(Eval::pieceValue(Phase(ph), WHITE, PieceType(pt)),
                      -Eval::pieceValue(Phase(ph), BLACK, PieceType(pt)));
        }
    }
}
TEST(Eval_psqValue, WhiteSquaresMirrorBlackSquares) {
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
// --- End tests for piece / square values ---

// --- Tests for Eval::passedPawn---
TEST(Eval_passedPawns, StartPosition) {
    Board b(STARTFEN);
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::passedPawns<WHITE>(wPawns, bPawns), 0);
    EXPECT_EQ(Eval::passedPawns<BLACK>(bPawns, wPawns), 0);
}
TEST(Eval_passedPawns, NoPassedPawns) {
    Board b("4k3/p2p4/8/8/8/8/P1P5/4K3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::passedPawns<WHITE>(wPawns, bPawns), 0);
    EXPECT_EQ(Eval::passedPawns<BLACK>(bPawns, wPawns), 0);
}
TEST(Eval_passedPawns, HasPassedPawns) {
    Board b("4k3/p3p3/8/8/8/8/P1P5/4K3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::passedPawns<WHITE>(wPawns, bPawns), BB::set(C2));
    EXPECT_EQ(Eval::passedPawns<BLACK>(bPawns, wPawns), BB::set(E7));
}
// --- End tests for Eval::passedPawn ---

// --- Tests for Eval::isolatedPawns ---
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
// --- End tests for Eval::isolatedPawns ---

// --- Tests for Eval::backwardsPawns ---
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
// --- End tests for Eval::backwardsPawns ---

// --- Tests for Eval::doubledPawns ---
TEST(Eval_doubledPawns, StartPositionNoDoubledPawns) {
    Board b(STARTFEN);
    EXPECT_EQ(Eval::doubledPawns<WHITE>(b.getPieces<PAWN>(WHITE)), 0);
    EXPECT_EQ(Eval::doubledPawns<BLACK>(b.getPieces<PAWN>(BLACK)), 0);
}
TEST(Eval_doubledPawns, WhiteDoubledPawnOnA4) {
    Board b("4k3/8/pp6/p7/P7/8/P7/4K3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::doubledPawns<WHITE>(wPawns), BB::set(A4));
    EXPECT_EQ(Eval::doubledPawns<BLACK>(bPawns), 0);
}
// --- End tests for Eval::doubledPawns ---

// --- Tests for Eval::blockedPawns ---
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
// --- End tests for Eval::blockedPawns ---

// --- Tests for Eval::outpostSquares ---
TEST(Eval_outpostSquares, StartPosition) {
    Board b(STARTFEN);
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::outpostSquares<WHITE>(wPawns, bPawns), 0);
    EXPECT_EQ(Eval::outpostSquares<BLACK>(bPawns, wPawns), 0);
}
TEST(Eval_outpostSquares, EmptyPosition) {
    Board b(EMPTYFEN);
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::outpostSquares<WHITE>(wPawns, bPawns), 0);
    EXPECT_EQ(Eval::outpostSquares<BLACK>(bPawns, wPawns), 0);
}
TEST(Eval_outpostSquares, WhiteOutpostOnD5) {
    Board b("r4rk1/pp3ppp/3p2n1/2p5/4P3/2N5/PPP2PPP/2KRR3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::outpostSquares<WHITE>(wPawns, bPawns), BB::set(D5));
    EXPECT_EQ(Eval::outpostSquares<BLACK>(bPawns, wPawns), 0);
}
TEST(Eval_outpostSquares, BlackOutpostOnD4) {
    Board b("r4rk1/pp2pppp/3pn3/2p5/2P1P3/1N6/PP3PPP/2KRR3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::outpostSquares<WHITE>(wPawns, bPawns), 0);
    EXPECT_EQ(Eval::outpostSquares<BLACK>(bPawns, wPawns), BB::set(D4));
}
TEST(Eval_outpostSquares, NoOupostOn7thRank) {
    Board b("r4rk1/1p2pppp/1P1pn3/2p5/8/pNPPP3/P4PPP/2KRR3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    EXPECT_EQ(Eval::outpostSquares<WHITE>(wPawns, bPawns), 0);
    EXPECT_EQ(Eval::outpostSquares<BLACK>(bPawns, wPawns), 0);
}
// --- End tests for Eval::oupostSquares ---

// --- Tests for Eval::bishopPawnBlockers ---
TEST(Eval_bishopPawnBlockers, StartPosition) {
    Board b(STARTFEN);
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 8);
    EXPECT_EQ(bBlockers, 8);
}
TEST(Eval_bishopPawnBlockers, EmptyPosition) {
    Board b(EMPTYFEN);
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 0);
    EXPECT_EQ(bBlockers, 0);
}
TEST(Eval_bishopPawnBlockers, DarkSquareBishopsDefaultPawns) {
    Board b("4kb2/pppppppp/8/8/8/8/PPPPPPPP/2B1K3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 4);
    EXPECT_EQ(bBlockers, 4);
}
TEST(Eval_bishopPawnBlockers, LightSquareBishopsDefaultPawns) {
    Board b("2b1k3/pppppppp/8/8/8/8/PPPPPPPP/4KB2 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 4);
    EXPECT_EQ(bBlockers, 4);
}
TEST(Eval_bishopPawnBlockers, LightSquareBishopsDarkSquarePawns) {
    Board b("2b1k3/p1p1p1p1/1p1p1p1p/8/8/P1P1P1P1/1P1P1P1P/4KB2 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 0);
    EXPECT_EQ(bBlockers, 0);
}
TEST(Eval_bishopPawnBlockers, DarkSquareBishopsDarkSquarePawns) {
    Board b("4kb2/p1p1p1p1/1p1p1p1p/8/8/P1P1P1P1/1P1P1P1P/2B1K3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 8);
    EXPECT_EQ(bBlockers, 8);
}
TEST(Eval_bishopPawnBlockers, DarkSquareBishopsLightSquarePawns) {
    Board b("4kb2/1p1p1p1p/p1p1p1p1/8/8/1P1P1P1P/P1P1P1P1/2B1K3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 0);
    EXPECT_EQ(bBlockers, 0);
}
TEST(Eval_bishopPawnBlockers, LightSquareBishopsLightSquarePawns) {
    Board b("2b1k3/1p1p1p1p/p1p1p1p1/8/8/1P1P1P1P/P1P1P1P1/4KB2 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 8);
    EXPECT_EQ(bBlockers, 8);
}
TEST(Eval_bishopPawnBlockers, BlockedCentralPawn) {
    Board b("4k3/4b3/8/4p3/4P3/8/4B3/4K3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 2);
    EXPECT_EQ(bBlockers, 2);
}
TEST(Eval_bishopPawnBlockers, BlockedOutsidePawn) {
    Board b("4k3/6b1/8/6p1/6P1/8/6B1/4K3 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 1);
    EXPECT_EQ(bBlockers, 1);
}
TEST(Eval_bishopPawnBlockers, MixedWithoutBlockers) {
    Board b("rn1qkbnr/ppp1pppp/3p4/8/8/4P3/PPPP1PPP/RN1QKBNR w KQkq - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 3);
    EXPECT_EQ(bBlockers, 5);
}
TEST(Eval_bishopPawnBlockers, MixedWithBlockers) {
    Board b("4kb2/5p1p/pp2p1p1/2pp4/2PP4/1P2PP1P/P5P1/4KB2 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 18);
    EXPECT_EQ(bBlockers, 6);
}
TEST(Eval_bishopPawnBlockers, DefendedWithBlocked) {
    Board b("6k1/8/8/3Bp3/3bP3/8/8/6K1 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 1);
    EXPECT_EQ(bBlockers, 1);
}
TEST(Eval_bishopPawnBlockers, DefendedWithNotBlocked) {
    Board b("6k1/8/8/3Bp3/2Pb4/8/8/6K1 w - - 0 1");
    U64 wPawns = b.getPieces<PAWN>(WHITE);
    U64 bPawns = b.getPieces<PAWN>(BLACK);
    U64 wBlockers = Eval::bishopPawnBlockers<WHITE>(b.getPieces<BISHOP>(WHITE), wPawns, bPawns);
    U64 bBlockers = Eval::bishopPawnBlockers<BLACK>(b.getPieces<BISHOP>(BLACK), bPawns, wPawns);
    EXPECT_EQ(wBlockers, 0);
    EXPECT_EQ(bBlockers, 0);
}
// --- End tests for Eval::bishopPawnBlockers ---
