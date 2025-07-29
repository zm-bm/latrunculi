#include "evaluator.hpp"

#include <gtest/gtest.h>

#include "bb.hpp"
#include "board.hpp"

#include "defs.hpp"
#include "eval.hpp"
#include "score.hpp"
#include "test_util.hpp"

class EvaluatorTest : public ::testing::Test {
protected:
    void test_outpost_zone(std::string fen, uint64_t w_expected, uint64_t b_expected) {
        Board     board(fen);
        Evaluator e(board);
        EXPECT_EQ(e.zones.outposts[WHITE], w_expected) << fen;
        EXPECT_EQ(e.zones.outposts[BLACK], b_expected) << fen;
    }

    void test_mobility_zone(std::string fen, uint64_t w_expected, uint64_t b_expected) {
        Board     board(fen);
        Evaluator e(board);
        EXPECT_EQ(e.zones.mobility[WHITE], w_expected) << fen;
        EXPECT_EQ(e.zones.mobility[BLACK], b_expected) << fen;
    }

    void test_mobility_score(const std::string fen, Score w_expected, Score b_expected) {
        Board     board(fen);
        Evaluator e(board);
        e.evaluate();
        EXPECT_EQ(e.scores.mobility[WHITE], w_expected) << fen;
        EXPECT_EQ(e.scores.mobility[BLACK], b_expected) << fen;
    }

    void test_evaluate_pawns(std::string fen, Score w_expected, Score b_expected) {
        Board     board(fen);
        Evaluator e(board);
        EXPECT_EQ(e.evaluate_pawns<WHITE>(), w_expected) << fen;
        EXPECT_EQ(e.evaluate_pawns<BLACK>(), b_expected) << fen;
    }

    template <PieceType p>
    void test_evaluate_pieces(std::string fen, Score w_expected, Score b_expected) {
        Board     board(fen);
        Evaluator e(board);
        Score     w_score = e.evaluate_pieces<WHITE, p>();
        Score     b_score = e.evaluate_pieces<BLACK, p>();
        EXPECT_EQ(w_score, w_expected) << fen;
        EXPECT_EQ(b_score, b_expected) << fen;
    }

    void test_king_safety(std::string fen, Score expected) {
        Board     board(fen);
        Evaluator e(board);
        e.evaluate();
        EXPECT_EQ(e.evaluate_king_safety<WHITE>(), expected) << fen;
        EXPECT_EQ(e.evaluate_king_safety<BLACK>(), expected) << fen;
    }

    void test_shelter(std::string fen, Score w_expected, Score b_expected) {
        Board     board(fen);
        Evaluator e(board);
        EXPECT_EQ(e.evaluate_shelter<WHITE>(board.king_sq(WHITE)), w_expected) << fen;
        EXPECT_EQ(e.evaluate_shelter<BLACK>(board.king_sq(BLACK)), b_expected) << fen;
    }

    void test_shelter_file(std::string fen, Score w_expected, Score b_expected, File file) {
        Board     board(fen);
        Evaluator e(board);
        uint64_t  w_pawns = board.pieces<PAWN>(WHITE);
        uint64_t  b_pawns = board.pieces<PAWN>(BLACK);
        EXPECT_EQ(e.evaluate_shelter_file<WHITE>(w_pawns, b_pawns, file), w_expected) << fen;
        EXPECT_EQ(e.evaluate_shelter_file<BLACK>(b_pawns, w_pawns, file), b_expected) << fen;
    }

    void test_raw_danger(std::string fen, int w_expected, int b_expected) {
        Board     board(fen);
        Evaluator e(board);
        e.evaluate();
        EXPECT_EQ(e.calculate_raw_danger<WHITE>(board.king_sq(WHITE)), w_expected) << fen;
        EXPECT_EQ(e.calculate_raw_danger<BLACK>(board.king_sq(BLACK)), b_expected) << fen;
    }

    void test_phase(std::string fen, int expected, int tolerance) {
        Board     board(fen);
        Evaluator e(board);
        EXPECT_LE(std::abs(e.phase() - expected), tolerance) << fen;
    }

    void test_scale_factor(std::string fen, float expected) {
        Board     board(fen);
        Evaluator e(board);
        EXPECT_EQ(e.scale_factor(board.side_to_move()), expected) << fen;
    }

    void test_taper_score(std::string fen, Score score, int expected) {
        Board     board(fen);
        Evaluator e(board);
        EXPECT_EQ(e.taper_score(score), expected) << fen;
    }
};

TEST_F(EvaluatorTest, Evaluate) {
    std::vector<std::tuple<std::string, int>> test_cases = {
        {EMPTYFEN, 0},
        {STARTFEN, 0},
    };

    for (const auto& [fen, expected] : test_cases) {
        Board board(fen);
        int   result = evaluate(board);
        EXPECT_EQ(result, expected + eval::tempo_bonus) << fen;
        board.make_null();
        EXPECT_EQ(result, expected + eval::tempo_bonus) << fen;
    }
}

TEST_F(EvaluatorTest, OutpostZone) {
    std::vector<std::tuple<std::string, uint64_t, uint64_t>> test_cases = {
        {STARTFEN, 0, 0},
        {EMPTYFEN, 0, 0},
        {"r4rk1/1p2pppp/1P1pn3/2p5/8/pNPPP3/P4PPP/2KRR3 w - - 0 1", 0, 0},
        {"r4rk1/pp3ppp/3p2n1/2p5/4P3/2N5/PPP2PPP/2KRR3 w - - 0 1", bb::set(D5), 0},
        {"r4rk1/pp2pppp/3pn3/2p5/2P1P3/1N6/PP3PPP/2KRR3 w - - 0 1", 0, bb::set(D4)},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_outpost_zone(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluatorTest, MobilityZone) {
    uint64_t white = bb::rank(RANK2) | bb::rank(RANK6) | bb::set(E1);
    uint64_t black = bb::rank(RANK7) | bb::rank(RANK3) | bb::set(E8);

    std::vector<std::tuple<std::string, uint64_t, uint64_t>> test_cases = {
        {STARTFEN, ~white, ~black},
        {EMPTYFEN, ~bb::set(E1), ~bb::set(E8)},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_mobility_zone(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluatorTest, MobilityScore) {
    std::vector<std::tuple<std::string, Score>> test_cases = {
        {EMPTYFEN, {0}},
        // no mobility area restriction
        {"3nk3/8/8/8/8/8/8/3NK3 w - - 0 1", eval::knight_mob[4]},
        {"3bk3/8/8/8/8/8/8/3BK3 w - - 0 2", eval::bishop_mob[7]},
        {"3rk3/8/8/8/8/8/8/3RK3 w - - 0 3", eval::rook_mob[10]},
        {"3qk3/8/8/8/8/8/8/3QK3 w - - 0 4", eval::queen_mob[17]},
        // with mobility area restriction
        {"3nk3/1p6/8/3P4/3p4/8/1P6/3NK3 w - - 0 5", eval::knight_mob[1]},
        {"3bk3/4p3/8/1p6/1P6/8/4P3/3BK3 w - - 0 6", eval::bishop_mob[2]},
        {"3rk3/P2p4/8/8/8/8/p2P4/3RK3 w - - 0 7", eval::rook_mob[2]},
        {"3qk3/P2pp3/8/1p6/1P6/8/p2PP3/3QK3 w - - 0 8", eval::queen_mob[4]},

    };

    for (const auto [fen, expected] : test_cases) {
        test_mobility_score(fen, expected, expected);
    }
}

TEST_F(EvaluatorTest, EvaluatePawns) {
    auto iso1        = "4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1";
    auto iso2        = "rnbqkbnr/ppppp1pp/8/8/8/8/P1PPPPPP/RNBQKBNR w KQkq - 0 2";
    auto iso3        = "rnbqkbnr/pppppp1p/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 3";
    auto backward1   = "4k3/8/3p4/2p5/2P5/1P6/8/4K3 w - - 0 4";
    auto backward2   = "4k3/8/8/2pp4/2P5/1P6/8/4K3 w - - 0 5";
    auto backward3   = "4k3/8/3p4/2p5/1PP5/8/8/4K3 w - - 0 6";
    auto doubled1    = "4k3/5pp1/4p3/3p4/3PP3/4P3/5PP1/4K3 w - - 0 7";
    auto doubled2    = "4k3/5pp1/4p3/3pp3/3P4/4P3/5PP1/4K3 w - - 0 8";
    auto iso_doubled = "k7/8/8/8/8/P7/P7/K7 w KQkq - 0 9";

    std::vector<std::tuple<std::string, Score, Score>> test_cases = {
        // sanity check
        {EMPTYFEN, Score::Zero, Score::Zero},
        {STARTFEN, Score::Zero, Score::Zero},
        // isolated pawns
        {iso1, eval::iso_pawn, eval::iso_pawn},
        {iso2, eval::iso_pawn, Score::Zero},
        {iso3, Score::Zero, eval::iso_pawn},
        // backwards pawns
        {backward1, eval::backward_pawn, eval::backward_pawn},
        {backward2, eval::backward_pawn, Score::Zero},
        {backward3, Score::Zero, eval::backward_pawn},
        // doubled pawns
        {doubled1, eval::doubled_pawn, Score::Zero},
        {doubled2, Score::Zero, eval::doubled_pawn},
        // isolated and doubled pawns
        {iso_doubled, eval::iso_pawn * 2 + eval::doubled_pawn, Score::Zero},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_evaluate_pawns(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluatorTest, KnightsScore) {
    std::vector<std::tuple<std::string, Score, Score>> test_cases = {
        {EMPTYFEN, Score::Zero, Score::Zero},
        {STARTFEN, eval::minor_pawn_shield * 2, eval::minor_pawn_shield * 2},
        // knight outposts
        {"6k1/8/2p5/4pNp1/3nP1P1/2P5/8/6K1 w - - 0 1", eval::knight_outpost, Score::Zero},
        {"6k1/8/2p5/3Np1p1/4PnP1/2P5/8/6K1 w - - 0 2", Score::Zero, eval::knight_outpost},
        // knight with reachable outposts
        {"6k1/8/2p5/1n2p1p1/4P1PN/2P5/8/6K1 w - - 0 3", eval::reachable_outpost, Score::Zero},
        {"6k1/8/2p5/4p1pn/1N2P1P1/2P5/8/6K1 w - - 0 4", Score::Zero, eval::reachable_outpost},
        // knight behind pawn
        {"6k1/8/4p3/8/8/4P3/4N3/6K1 w - - 0 5", eval::minor_pawn_shield, Score::Zero},
        {"6k1/4n3/4p3/8/8/4P3/8/6K1 w - - 0 6", Score::Zero, eval::minor_pawn_shield},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_evaluate_pieces<KNIGHT>(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluatorTest, BishopsScore) {
    Score startScore  = eval::minor_pawn_shield * 2 + eval::bishop_pair + eval::bishop_blockers * 8,
          hasOutpost  = eval::bishop_outpost + eval::bishop_blockers * 2,
          noOutpost   = eval::bishop_blockers * 4,
          hasLongDiag = eval::bishop_long_diag + eval::bishop_blockers,
          noLongDiag  = eval::bishop_blockers * 2,
          twoPawnsDefended   = eval::bishop_blockers * 2 + eval::bishop_outpost,
          twoPawnsOneBlocked = eval::bishop_blockers * 4,
          twoPawnsTwoBlocked = eval::bishop_blockers * 6;

    std::vector<std::tuple<std::string, Score, Score>> test_cases = {
        {EMPTYFEN, Score::Zero, Score::Zero},
        {STARTFEN, startScore, startScore},
        // bishop outposts
        {"6k1/8/2p5/4pBp1/4P1P1/2P3b1/8/6K1 w - - 0 1", hasOutpost, noOutpost},
        {"6k1/8/2p3B1/4p1p1/4PbP1/2P5/8/6K1 w - - 0 2", noOutpost, hasOutpost},
        // bishop behind pawn
        {"6k1/8/4p3/8/8/4P3/4B3/6K1 w - - 0 3", eval::minor_pawn_shield, Score::Zero},
        {"6k1/4b3/4p3/8/8/4P3/8/6K1 w - - 0 4", Score::Zero, eval::minor_pawn_shield},
        // bishop on long diagonal
        {"6k1/6b1/8/3P4/3p4/8/6B1/6K1 w - - 0 5", hasLongDiag, hasLongDiag},
        {"6k1/6b1/8/4p3/4P3/8/6B1/6K1 w - - 0 6", noLongDiag, noLongDiag},
        // bishop pair
        {"5bk1/8/8/8/8/8/8/4BBK1 w - - 0 7", eval::bishop_pair, Score::Zero},
        {"4bbk1/8/8/8/8/8/8/5BK1 w - - 0 8", Score::Zero, eval::bishop_pair},
        // bishop/pawn penalty
        {"4k3/8/8/2BPp3/2bpP3/8/8/4K3 w - - 0 9", Score::Zero, Score::Zero},
        {"4k3/8/8/2bPp3/2BpP3/8/8/4K3 w - - 0 10", twoPawnsOneBlocked, twoPawnsOneBlocked},
        {"4k3/8/8/3PpB2/3pPb2/8/8/4K3 w - - 0 11", twoPawnsDefended, twoPawnsDefended},
        {"4k3/4b3/8/4p3/3pP3/3P4/4B3/4K3 w - - 0 12", twoPawnsTwoBlocked, twoPawnsTwoBlocked},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_evaluate_pieces<BISHOP>(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluatorTest, RookScore) {
    std::vector<std::tuple<std::string, Score, Score>> test_cases = {
        {STARTFEN, Score::Zero, Score::Zero},
        {EMPTYFEN, Score::Zero, Score::Zero},
        {"6kr/8/8/8/8/8/8/RK6 w - - 0 1", eval::rook_open_file[1], eval::rook_open_file[1]},
        {"6kr/p7/8/8/8/8/7P/RK6 w - - 0 2", eval::rook_open_file[0], eval::rook_open_file[0]},
        {"rn5k/8/8/p7/P7/8/8/RN5K w - - 0 3", eval::rook_closed_file, eval::rook_closed_file},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_evaluate_pieces<ROOK>(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluatorTest, QueenScore) {
    std::vector<std::tuple<std::string, Score, Score>> test_cases = {
        {STARTFEN, Score::Zero, Score::Zero},
        {EMPTYFEN, Score::Zero, Score::Zero},
        // bishop discovered attack
        {"3qk3/2P5/1P6/B7/b7/1p6/8/3QK3 w - - 0 1", eval::queen_discover_att, Score::Zero},
        {"3qk3/8/1P6/B7/b7/1p6/2p5/3QK3 w - - 0 2", Score::Zero, eval::queen_discover_att},
        // rook discovered attack
        {"RNNqk3/8/8/8/8/8/8/rn1QK3 w - - 0 3", eval::queen_discover_att, Score::Zero},
        {"RN1qk3/8/8/8/8/8/8/rnnQK3 w - - 0 4", Score::Zero, eval::queen_discover_att},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_evaluate_pieces<QUEEN>(fen, w_expected, b_expected);
    }
}

Score shelter(const std::vector<Rank>& shelter_ranks,
              const std::vector<Rank>& storm_ranks,
              const std::vector<Rank>& blocked_ranks) {
    Score score;
    for (auto r : shelter_ranks)
        score += eval::pawn_shelter[r];
    for (auto r : storm_ranks)
        score += eval::pawn_storm[0][r];
    for (auto r : blocked_ranks)
        score += eval::pawn_storm[1][r];
    return score;
}

TEST_F(EvaluatorTest, KingSafety) {
    Score empty = shelter({RANK1, RANK1, RANK1}, {RANK1, RANK1, RANK1}, {}) +
                  eval::king_file[FILE5] + eval::king_open_file[true][true];
    Score start = shelter({RANK2, RANK2, RANK2}, {RANK7, RANK7, RANK7}, {}) +
                  eval::king_file[FILE7] + eval::king_open_file[false][false];

    std::vector<std::tuple<std::string, Score>> test_cases = {
        {EMPTYFEN, empty},
        {STARTFEN, start},
    };

    for (const auto& [fen, expected] : test_cases) {
        test_king_safety(fen, expected);
    }
}

TEST_F(EvaluatorTest, Shelter) {
    Score empty = shelter({RANK1, RANK1, RANK1}, {RANK1, RANK1, RANK1}, {}) +
                  eval::king_file[int(FILE5)] + eval::king_open_file[true][true];
    Score start = shelter({RANK2, RANK2, RANK2}, {RANK7, RANK7, RANK7}, {}) +
                  eval::king_file[int(FILE5)] + eval::king_open_file[false][false];
    Score blocked = shelter({RANK3, RANK4, RANK5}, {RANK6, RANK4}, {RANK5}) +
                    eval::king_file[int(FILE1)] + eval::king_open_file[false][false];
    Score semiopen1 = shelter({RANK2, RANK2, RANK2}, {RANK1, RANK1, RANK1}, {}) +
                      eval::king_file[int(FILE1)] + eval::king_open_file[false][true];
    Score semiopen2 = shelter({RANK1, RANK1, RANK1}, {RANK7, RANK7, RANK7}, {}) +
                      eval::king_file[int(FILE1)] + eval::king_open_file[true][false];
    Score rank2 = shelter({RANK1, RANK1, RANK3}, {RANK7, RANK7, RANK6}, {}) +
                  eval::king_file[int(FILE2)] + eval::king_open_file[false][false];
    Score attacked = shelter({RANK2, RANK2, RANK1}, {RANK7, RANK7, RANK7}, {}) +
                     eval::king_file[int(FILE1)] + eval::king_open_file[false][false];

    std::vector<std::tuple<std::string, Score, Score>> test_cases = {
        {EMPTYFEN, empty, empty},
        {STARTFEN, start, start},
        {"k7/8/p7/1pP5/1Pp5/P7/8/K7 w - - 0 1", blocked, blocked},
        {"7k/5ppp/8/8/8/8/PPP5/K7 w - - 0 2", semiopen1, semiopen1},
        {"k7/5ppp/8/8/8/8/PPP5/7K w - - 0 3", semiopen2, semiopen2},
        {"8/5pkp/6p1/8/8/6P1/5PKP/8 w - - 0 4", rank2, rank2},
        {"k7/ppp5/3P4/8/8/3p4/PPP5/K7 w - - 0 5", attacked, attacked},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_shelter(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluatorTest, FileShelter) {
    Score empty   = shelter({RANK1}, {RANK1}, {});
    Score start   = shelter({RANK2}, {RANK7}, {});
    Score blocked = shelter({RANK4}, {}, {RANK5});

    std::vector<std::tuple<std::string, Score, Score, File>> test_cases = {
        {EMPTYFEN, empty, empty, FILE5},
        {STARTFEN, start, start, FILE5},
        {"1k6/8/8/1p6/1P6/8/8/1K6 w - - 0 1", blocked, blocked, FILE2},
    };

    for (const auto& [fen, w_expected, b_expected, file] : test_cases) {
        test_shelter_file(fen, w_expected, b_expected, file);
    }
}

TEST_F(EvaluatorTest, RawDanger) {
    int danger = (eval::safe_check_danger[QUEEN] + eval::safe_check_danger[BISHOP] +
                  eval::kingzone_att_danger[QUEEN] + eval::weak_kingzone_danger);

    std::vector<std::tuple<std::string, int, int>> test_cases = {
        // No danger
        {EMPTYFEN, 0, 0},
        {STARTFEN, 0, 0},
        // unsafe rook checks
        {"4k3/5n2/8/8/8/8/4P3/4K1NR w - - 0 2", 0, eval::unsafe_check_danger[ROOK]},
        {"4k1nr/4p3/8/8/8/8/5N2/4K3 w - - 0 3", eval::unsafe_check_danger[ROOK], 0},
        // safe queen + bishop checks
        {"r1n1kn1r/8/8/8/8/8/8/R2QKB2 w - - 0 4", 0, danger},
        {"r2qkb2/8/8/8/8/8/8/R1N1KN1R w - - 0 5", danger, 0},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_raw_danger(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluatorTest, ScaleFactor) {
    std::vector<std::pair<std::string, float>> test_cases = {
        {EMPTYFEN, 36.0 / eval::scale_limit},
        {STARTFEN, 1.0},
        {"4k3/8/8/8/8/8/4P3/4K3 w K - 0 1", 41.0 / eval::scale_limit}, // Single pawn
    };

    for (const auto& [fen, expected] : test_cases) {
        test_scale_factor(fen, expected);
    }
}

TEST_F(EvaluatorTest, TaperScore) {
    std::vector<std::tuple<std::string, Score, int>> test_cases = {
        {EMPTYFEN, {100, 200}, 200},
        {STARTFEN, {100, 200}, 100},
    };

    for (const auto& [fen, score, expected] : test_cases) {
        test_taper_score(fen, score, expected);
    }
}

TEST_F(EvaluatorTest, Phase) {
    std::vector<std::tuple<std::string, int, int>> test_cases = {
        {STARTFEN, eval::phase_limit, 0},
        {EMPTYFEN, 0, 0},
        {"krrnBRRK/8/8/8/8/8/8/8 w - - 0 1", 50, 10},
        {"kr4RK/8/8/8/8/8/8/8 w - - 0 1", 0, 0},
    };

    for (const auto& [fen, expected, tolerance] : test_cases) {
        test_phase(fen, expected, tolerance);
    }
}
