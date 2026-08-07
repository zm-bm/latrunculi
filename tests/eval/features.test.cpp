#include "eval/evaluation.hpp"

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "core/bitboard.hpp"
#include "eval/parameters.hpp"
#include "eval/tapered_score.hpp"
#include "support/board_fixtures.hpp"
#include "support/evaluator_test_access.hpp"

class EvaluationFeaturesTest : public ::testing::Test {
protected:
    void test_outpost_zone(std::string fen, Bitboard w_expected, Bitboard b_expected) {
        const Board board(fen);
        EXPECT_EQ(EvaluatorTestAccess::outposts(board, WHITE), w_expected) << fen;
        EXPECT_EQ(EvaluatorTestAccess::outposts(board, BLACK), b_expected) << fen;
    }

    void test_mobility_zone(std::string fen, Bitboard w_expected, Bitboard b_expected) {
        const Board board(fen);
        EXPECT_EQ(EvaluatorTestAccess::mobility_zone(board, WHITE), w_expected) << fen;
        EXPECT_EQ(EvaluatorTestAccess::mobility_zone(board, BLACK), b_expected) << fen;
    }

    void test_mobility_score(const std::string  fen,
                             eval::TaperedScore w_expected,
                             eval::TaperedScore b_expected) {
        test_term_score(fen, eval::Term::Mobility, w_expected, b_expected);
    }

    template <Color C, PieceType P>
    Bitboard test_piece_moves(const std::string& fen, Square sq) {
        const Board board(fen);
        return EvaluatorTestAccess::piece_moves<C, P>(board, sq);
    }

    void test_threat_score(const std::string& fen,
                           eval::TaperedScore w_expected,
                           eval::TaperedScore b_expected) {
        test_term_score(fen, eval::Term::Threats, w_expected, b_expected);
    }

    void test_evaluate_pawns(std::string        fen,
                             eval::TaperedScore w_expected,
                             eval::TaperedScore b_expected) {
        test_term_score(fen, eval::Term::Pawns, w_expected, b_expected);
    }

    template <PieceType p>
    void test_evaluate_pieces(std::string        fen,
                              eval::TaperedScore w_expected,
                              eval::TaperedScore b_expected) {
        constexpr eval::Term term = p == KNIGHT ? eval::Term::Knights
                                  : p == BISHOP ? eval::Term::Bishops
                                  : p == ROOK   ? eval::Term::Rooks
                                                : eval::Term::Queens;
        test_term_score(fen, term, w_expected, b_expected);
    }

    void test_king_safety(std::string fen, eval::TaperedScore expected) {
        test_term_score(fen, eval::Term::King, expected, expected);
    }

    void
    test_shelter(std::string fen, eval::TaperedScore w_expected, eval::TaperedScore b_expected) {
        const Board board(fen);
        EXPECT_EQ(EvaluatorTestAccess::shelter<WHITE>(board, board.king_sq(WHITE)), w_expected)
            << fen;
        EXPECT_EQ(EvaluatorTestAccess::shelter<BLACK>(board, board.king_sq(BLACK)), b_expected)
            << fen;
    }

    void test_shelter_file(std::string        fen,
                           eval::TaperedScore w_expected,
                           eval::TaperedScore b_expected,
                           File               file) {
        const Board    board(fen);
        const Bitboard w_pawns = board.pieces<PAWN>(WHITE);
        const Bitboard b_pawns = board.pieces<PAWN>(BLACK);
        EXPECT_EQ(EvaluatorTestAccess::shelter_file<WHITE>(board, w_pawns, b_pawns, file),
                  w_expected)
            << fen;
        EXPECT_EQ(EvaluatorTestAccess::shelter_file<BLACK>(board, b_pawns, w_pawns, file),
                  b_expected)
            << fen;
    }

    void test_raw_danger(std::string fen, int w_expected, int b_expected) {
        const Board board(fen);
        EXPECT_EQ(EvaluatorTestAccess::raw_danger<WHITE>(board, board.king_sq(WHITE)), w_expected)
            << fen;
        EXPECT_EQ(EvaluatorTestAccess::raw_danger<BLACK>(board, board.king_sq(BLACK)), b_expected)
            << fen;
    }

    void test_phase(std::string fen, int expected, int tolerance) {
        const Board board(fen);
        EXPECT_LE(std::abs(EvaluatorTestAccess::phase(board) - expected), tolerance) << fen;
    }

    void test_scale_factor(std::string fen, int expected) {
        const Board board(fen);
        EXPECT_EQ(EvaluatorTestAccess::scale_factor(board, board.side_to_move()), expected) << fen;
    }

    void test_taper_score(std::string fen, eval::TaperedScore score, int expected) {
        const Board board(fen);
        EXPECT_EQ(EvaluatorTestAccess::taper_score(board, score), expected) << fen;
    }

    void test_term_score(const std::string& fen,
                         eval::Term         term,
                         eval::TaperedScore white,
                         eval::TaperedScore black) {
        const Board       board(fen);
        const eval::Trace trace = eval::evaluate_trace(board);
        EXPECT_EQ(trace.term(term).white, white) << fen;
        EXPECT_EQ(trace.term(term).black, black) << fen;
    }
};

TEST_F(EvaluationFeaturesTest, OutpostZone) {
    std::vector<std::tuple<std::string, Bitboard, Bitboard>> test_cases = {
        {board_test::fen::start, 0, 0},
        {board_test::fen::kings_only, 0, 0},
        {"r4rk1/1p2pppp/1P1pn3/2p5/8/pNPPP3/P4PPP/2KRR3 w - - 0 1", 0, 0},
        {"r4rk1/pp3ppp/3p2n1/2p5/4P3/2N5/PPP2PPP/2KRR3 w - - 0 1", bb::set(D5), 0},
        {"r4rk1/pp2pppp/3pn3/2p5/2P1P3/1N6/PP3PPP/2KRR3 w - - 0 1", 0, bb::set(D4)},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_outpost_zone(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluationFeaturesTest, MobilityZone) {
    Bitboard white = bb::rank(RANK2) | bb::rank(RANK6) | bb::set(E1);
    Bitboard black = bb::rank(RANK7) | bb::rank(RANK3) | bb::set(E8);

    std::vector<std::tuple<std::string, Bitboard, Bitboard>> test_cases = {
        {board_test::fen::start, ~white, ~black},
        {board_test::fen::kings_only, ~bb::set(E1), ~bb::set(E8)},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_mobility_zone(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluationFeaturesTest, MobilityScore) {
    std::vector<std::tuple<std::string, eval::TaperedScore>> test_cases = {
        {board_test::fen::kings_only, {0}},
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

TEST_F(EvaluationFeaturesTest, PinnedPieceMobilityStaysOnPinRay) {
    const Bitboard moves = test_piece_moves<WHITE, ROOK>(board_test::fen::pinned_rook, E2);

    EXPECT_EQ(moves & ~bb::file(FILE5), 0ULL);
    EXPECT_EQ(bb::count(moves), 7);
    EXPECT_TRUE(bb::contains(moves, E8));
}

TEST_F(EvaluationFeaturesTest, EvaluatePawns) {
    auto iso1        = "4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1";
    auto iso2        = "rnbqkbnr/ppppp1pp/8/8/8/8/P1PPPPPP/RNBQKBNR w KQkq - 0 2";
    auto iso3        = "rnbqkbnr/pppppp1p/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 3";
    auto backward1   = "4k3/8/3p4/2p5/2P5/1P6/8/4K3 w - - 0 4";
    auto backward2   = "4k3/8/8/2pp4/2P5/1P6/8/4K3 w - - 0 5";
    auto backward3   = "4k3/8/3p4/2p5/1PP5/8/8/4K3 w - - 0 6";
    auto doubled1    = "4k3/5pp1/4p3/3p4/3PP3/4P3/5PP1/4K3 w - - 0 7";
    auto doubled2    = "4k3/5pp1/4p3/3pp3/3P4/4P3/5PP1/4K3 w - - 0 8";
    auto iso_doubled = "k7/8/8/8/8/P7/P7/K7 w KQkq - 0 9";
    auto opposed     = "4k3/8/2p5/8/1P6/8/8/4K3 w - - 0 1";
    auto passed      = "4k3/8/8/6p1/1P6/8/8/4K3 w - - 0 1";
    auto advanced    = "4k3/P7/8/8/8/8/7p/4K3 w - - 0 1";

    auto iso_backward = "4k3/8/3p4/8/2P5/8/8/4K3 w - - 0 1";

    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore>> test_cases = {
        // sanity check
        {board_test::fen::kings_only, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        {board_test::fen::start, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        // isolated pawns
        {iso1, eval::iso_pawn, eval::iso_pawn},
        {iso2, eval::iso_pawn, eval::TaperedScore::Zero},
        {iso3, eval::TaperedScore::Zero, eval::iso_pawn},
        // backwards pawns
        {backward1, eval::backward_pawn, eval::backward_pawn},
        {backward2, eval::backward_pawn, eval::TaperedScore::Zero},
        {backward3, eval::TaperedScore::Zero, eval::backward_pawn},
        // isolated pawns are not also backwards
        {iso_backward, eval::iso_pawn, eval::iso_pawn},
        // doubled pawns
        {doubled1, eval::doubled_pawn, eval::TaperedScore::Zero},
        {doubled2, eval::TaperedScore::Zero, eval::doubled_pawn},
        // isolated and doubled pawns
        {iso_doubled,
         eval::iso_pawn * 2 + eval::doubled_pawn + eval::passed_pawn[RANK2]
             + eval::passed_pawn[RANK3],
         eval::TaperedScore::Zero},
        // passed pawns: opposed adjacent files, rank-four passers, advanced passers
        {opposed, eval::iso_pawn, eval::iso_pawn},
        {passed,
         eval::iso_pawn + eval::passed_pawn[RANK4],
         eval::iso_pawn + eval::passed_pawn[RANK4]},
        {advanced,
         eval::iso_pawn + eval::passed_pawn[RANK7],
         eval::iso_pawn + eval::passed_pawn[RANK7]},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_evaluate_pawns(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluationFeaturesTest, KnightsScore) {
    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore>> test_cases = {
        {board_test::fen::kings_only, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        {board_test::fen::start, eval::minor_pawn_shield * 2, eval::minor_pawn_shield * 2},
        // knight outposts
        {"6k1/8/2p5/4pNp1/3nP1P1/2P5/8/6K1 w - - 0 1",
         eval::knight_outpost,
         eval::TaperedScore::Zero},
        {"6k1/8/2p5/3Np1p1/4PnP1/2P5/8/6K1 w - - 0 2",
         eval::TaperedScore::Zero,
         eval::knight_outpost},
        // knight with reachable outposts
        {"6k1/8/2p5/1n2p1p1/4P1PN/2P5/8/6K1 w - - 0 3",
         eval::reachable_outpost,
         eval::TaperedScore::Zero},
        {"6k1/8/2p5/4p1pn/1N2P1P1/2P5/8/6K1 w - - 0 4",
         eval::TaperedScore::Zero,
         eval::reachable_outpost},
        // knight behind pawn
        {"6k1/8/4p3/8/8/4P3/4N3/6K1 w - - 0 5", eval::minor_pawn_shield, eval::TaperedScore::Zero},
        {"6k1/4n3/4p3/8/8/4P3/8/6K1 w - - 0 6", eval::TaperedScore::Zero, eval::minor_pawn_shield},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_evaluate_pieces<KNIGHT>(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluationFeaturesTest, BishopsScore) {
    eval::TaperedScore startScore = eval::minor_pawn_shield * 2 + eval::bishop_pair
                                  + eval::bishop_blockers * 8,
                       hasOutpost         = eval::bishop_outpost + eval::bishop_blockers * 2,
                       noOutpost          = eval::bishop_blockers * 4,
                       hasLongDiag        = eval::bishop_long_diag + eval::bishop_blockers,
                       noLongDiag         = eval::bishop_blockers * 2,
                       twoPawnsDefended   = eval::bishop_blockers * 2 + eval::bishop_outpost,
                       twoPawnsOneBlocked = eval::bishop_blockers * 4,
                       twoPawnsTwoBlocked = eval::bishop_blockers * 6;

    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore>> test_cases = {
        {board_test::fen::kings_only, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        {board_test::fen::start, startScore, startScore},
        // bishop outposts
        {"6k1/8/2p5/4pBp1/4P1P1/2P3b1/8/6K1 w - - 0 1", hasOutpost, noOutpost},
        {"6k1/8/2p3B1/4p1p1/4PbP1/2P5/8/6K1 w - - 0 2", noOutpost, hasOutpost},
        // bishop behind pawn
        {"6k1/8/4p3/8/8/4P3/4B3/6K1 w - - 0 3", eval::minor_pawn_shield, eval::TaperedScore::Zero},
        {"6k1/4b3/4p3/8/8/4P3/8/6K1 w - - 0 4", eval::TaperedScore::Zero, eval::minor_pawn_shield},
        // bishop on long diagonal
        {"6k1/6b1/8/3P4/3p4/8/6B1/6K1 w - - 0 5", hasLongDiag, hasLongDiag},
        {"6k1/6b1/8/4p3/4P3/8/6B1/6K1 w - - 0 6", noLongDiag, noLongDiag},
        // bishop pair
        {"5bk1/8/8/8/8/8/8/4BBK1 w - - 0 7", eval::bishop_pair, eval::TaperedScore::Zero},
        {"4bbk1/8/8/8/8/8/8/5BK1 w - - 0 8", eval::TaperedScore::Zero, eval::bishop_pair},
        // bishop/pawn penalty
        {"4k3/8/8/2BPp3/2bpP3/8/8/4K3 w - - 0 9",
         eval::TaperedScore::Zero,
         eval::TaperedScore::Zero},
        {"4k3/8/8/2bPp3/2BpP3/8/8/4K3 w - - 0 10", twoPawnsOneBlocked, twoPawnsOneBlocked},
        {"4k3/8/8/3PpB2/3pPb2/8/8/4K3 w - - 0 11", twoPawnsDefended, twoPawnsDefended},
        {"4k3/4b3/8/4p3/3pP3/3P4/4B3/4K3 w - - 0 12", twoPawnsTwoBlocked, twoPawnsTwoBlocked},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_evaluate_pieces<BISHOP>(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluationFeaturesTest, RookScore) {
    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore>> test_cases = {
        {board_test::fen::start, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        {board_test::fen::kings_only, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        {"6kr/8/8/8/8/8/8/RK6 w - - 0 1", eval::rook_open_file[1], eval::rook_open_file[1]},
        {"6kr/p7/8/8/8/8/7P/RK6 w - - 0 2", eval::rook_open_file[0], eval::rook_open_file[0]},
        {"rn5k/8/8/p7/P7/8/8/RN5K w - - 0 3", eval::rook_closed_file, eval::rook_closed_file},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_evaluate_pieces<ROOK>(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluationFeaturesTest, QueenScore) {
    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore>> test_cases = {
        {board_test::fen::start, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        {board_test::fen::kings_only, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        // a direct attack is not a discovered attack
        {"3rk3/8/8/8/8/8/8/3QK3 w - - 0 1", eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        // bishop discovered attack
        {"3qk3/2P5/1P6/B7/b7/1p6/8/3QK3 w - - 0 1",
         eval::queen_discover_att,
         eval::TaperedScore::Zero},
        {"3qk3/8/1P6/B7/b7/1p6/2p5/3QK3 w - - 0 2",
         eval::TaperedScore::Zero,
         eval::queen_discover_att},
        // rook discovered attack
        {"RNNqk3/8/8/8/8/8/8/rn1QK3 w - - 0 3", eval::queen_discover_att, eval::TaperedScore::Zero},
        {"RN1qk3/8/8/8/8/8/8/rnnQK3 w - - 0 4", eval::TaperedScore::Zero, eval::queen_discover_att},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_evaluate_pieces<QUEEN>(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluationFeaturesTest, ThreatScore) {
    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore>> test_cases = {
        {board_test::fen::kings_only, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        {"4k3/8/8/7b/8/8/r3N3/4K3 w - - 0 1", eval::weak_piece[KNIGHT], eval::TaperedScore::Zero},
        {"4k3/8/R3n3/8/7B/8/8/4K3 w - - 0 2", eval::TaperedScore::Zero, eval::weak_piece[KNIGHT]},
        {"4k3/8/8/2p5/3N4/2P5/8/4K3 w - - 0 1", eval::weak_piece[KNIGHT], eval::TaperedScore::Zero},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_threat_score(fen, w_expected, b_expected);
    }
}

eval::TaperedScore shelter(const std::vector<Rank>& shelter_ranks,
                           const std::vector<Rank>& storm_ranks,
                           const std::vector<Rank>& blocked_ranks) {
    eval::TaperedScore score;
    for (auto r : shelter_ranks)
        score += eval::pawn_shelter[r];
    for (auto r : storm_ranks)
        score += eval::pawn_storm[0][r];
    for (auto r : blocked_ranks)
        score += eval::pawn_storm[1][r];
    return score;
}

TEST_F(EvaluationFeaturesTest, KingSafety) {
    eval::TaperedScore empty = shelter({RANK1, RANK1, RANK1}, {RANK1, RANK1, RANK1}, {})
                             + eval::king_file[FILE5] + eval::king_open_file[true][true];
    eval::TaperedScore start = shelter({RANK2, RANK2, RANK2}, {RANK7, RANK7, RANK7}, {})
                             + eval::king_file[FILE7] + eval::king_open_file[false][false];

    std::vector<std::tuple<std::string, eval::TaperedScore>> test_cases = {
        {board_test::fen::kings_only, empty},
        {board_test::fen::start, start},
    };

    for (const auto& [fen, expected] : test_cases) {
        test_king_safety(fen, expected);
    }

    const Board with_rights("4k2r/8/8/8/8/8/5PPP/4K2R w K - 0 1");
    const Board without_rights("4k2r/8/8/8/8/8/5PPP/4K2R w - - 0 1");
    const auto  current_shelter = EvaluatorTestAccess::shelter<WHITE>(with_rights, E1);
    const auto  castled_shelter = EvaluatorTestAccess::shelter<WHITE>(with_rights, G1);

    ASSERT_GT(castled_shelter.mg, current_shelter.mg);
    const auto with_rights_score = eval::evaluate_trace(with_rights).term(eval::Term::King).white;
    const auto without_rights_score =
        eval::evaluate_trace(without_rights).term(eval::Term::King).white;
    EXPECT_EQ(with_rights_score - without_rights_score, castled_shelter - current_shelter);
}

TEST_F(EvaluationFeaturesTest, Shelter) {
    eval::TaperedScore empty = shelter({RANK1, RANK1, RANK1}, {RANK1, RANK1, RANK1}, {})
                             + eval::king_file[int(FILE5)] + eval::king_open_file[true][true];
    eval::TaperedScore start = shelter({RANK2, RANK2, RANK2}, {RANK7, RANK7, RANK7}, {})
                             + eval::king_file[int(FILE5)] + eval::king_open_file[false][false];
    eval::TaperedScore blocked = shelter({RANK3, RANK4, RANK5}, {RANK6, RANK4}, {RANK5})
                               + eval::king_file[int(FILE1)] + eval::king_open_file[false][false];
    eval::TaperedScore semiopen1 = shelter({RANK2, RANK2, RANK2}, {RANK1, RANK1, RANK1}, {})
                                 + eval::king_file[int(FILE1)] + eval::king_open_file[false][true];
    eval::TaperedScore semiopen2 = shelter({RANK1, RANK1, RANK1}, {RANK7, RANK7, RANK7}, {})
                                 + eval::king_file[int(FILE1)] + eval::king_open_file[true][false];
    eval::TaperedScore rank2 = shelter({RANK1, RANK1, RANK3}, {RANK7, RANK7, RANK6}, {})
                             + eval::king_file[int(FILE2)] + eval::king_open_file[false][false];
    eval::TaperedScore attacked = shelter({RANK2, RANK2, RANK1}, {RANK7, RANK7, RANK7}, {})
                                + eval::king_file[int(FILE1)] + eval::king_open_file[false][false];

    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore>> test_cases = {
        {board_test::fen::kings_only, empty, empty},
        {board_test::fen::start, start, start},
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

TEST_F(EvaluationFeaturesTest, FileShelter) {
    eval::TaperedScore empty   = shelter({RANK1}, {RANK1}, {});
    eval::TaperedScore start   = shelter({RANK2}, {RANK7}, {});
    eval::TaperedScore blocked = shelter({RANK4}, {}, {RANK5});

    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore, File>> test_cases =
        {
            {board_test::fen::kings_only, empty, empty, FILE5},
            {board_test::fen::start, start, start, FILE5},
            {"1k6/8/8/1p6/1P6/8/8/1K6 w - - 0 1", blocked, blocked, FILE2},
        };

    for (const auto& [fen, w_expected, b_expected, file] : test_cases) {
        test_shelter_file(fen, w_expected, b_expected, file);
    }
}

TEST_F(EvaluationFeaturesTest, RawDanger) {
    int danger = (eval::safe_check_danger[QUEEN] + eval::safe_check_danger[BISHOP]
                  + eval::kingzone_att_danger[QUEEN] + eval::weak_kingzone_danger);

    std::vector<std::tuple<std::string, int, int>> test_cases = {
        // No danger
        {board_test::fen::kings_only, 0, 0},
        {board_test::fen::start, 0, 0},
        // unsafe rook checks
        {"4k3/5n2/8/8/8/8/4P3/4K1NR w - - 0 2", 0, eval::unsafe_check_danger[ROOK]},
        {"4k1nr/4p3/8/8/8/8/5N2/4K3 w - - 0 3", eval::unsafe_check_danger[ROOK], 0},
        // An enemy piece blocking its own slider is not a pinned defender.
        {"4r2k/8/8/8/8/8/4n3/4K3 w - - 0 1", 203, 0},
        // safe queen + bishop checks
        {"r1n1kn1r/8/8/8/8/8/8/R2QKB2 w - - 0 4", 0, danger},
        {"r2qkb2/8/8/8/8/8/8/R1N1KN1R w - - 0 5", danger, 0},
    };

    for (const auto& [fen, w_expected, b_expected] : test_cases) {
        test_raw_danger(fen, w_expected, b_expected);
    }
}

TEST_F(EvaluationFeaturesTest, ScaleFactor) {
    std::vector<std::pair<std::string, int>> test_cases = {
        {board_test::fen::kings_only, 48},
        {board_test::fen::start, eval::scale_limit},
        {"4k3/8/8/8/8/8/4P3/4K3 w K - 0 1", 52}, // Single pawn
    };

    for (const auto& [fen, expected] : test_cases) {
        test_scale_factor(fen, expected);
    }
}

TEST_F(EvaluationFeaturesTest, TaperScore) {
    std::vector<std::tuple<std::string, eval::TaperedScore, int>> test_cases = {
        {board_test::fen::kings_only, {100, 200}, 200},
        {board_test::fen::start, {100, 200}, 100},
    };

    for (const auto& [fen, score, expected] : test_cases) {
        test_taper_score(fen, score, expected);
    }
}

TEST_F(EvaluationFeaturesTest, Phase) {
    std::vector<std::tuple<std::string, int, int>> test_cases = {
        {board_test::fen::start, eval::phase_limit, 0},
        {board_test::fen::kings_only, 0, 0},
        {"krrnBRRK/8/8/8/8/8/8/8 w - - 0 1", 50, 10},
        {"kr4RK/8/8/8/8/8/8/8 w - - 0 1", 19, 0},
    };

    for (const auto& [fen, expected, tolerance] : test_cases) {
        test_phase(fen, expected, tolerance);
    }
}
