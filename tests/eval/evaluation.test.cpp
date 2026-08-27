#include "eval/evaluation.hpp"

#include <array>
#include <initializer_list>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "eval/features.hpp"
#include "eval/parameters.hpp"
#include "eval/tapered_score.hpp"
#include "support/board_fixtures.hpp"

namespace {

void expect_term_score(std::string_view   fen,
                       eval::Term         term,
                       eval::TaperedScore white,
                       eval::TaperedScore black) {
    const Board               board(fen);
    const eval::FeatureRecord record = eval::extract_features(board);
    EXPECT_EQ(record.term(term).white, white) << fen;
    EXPECT_EQ(record.term(term).black, black) << fen;
}

eval::TaperedScore shelter(std::initializer_list<Rank> shelter_ranks,
                           std::initializer_list<Rank> storm_ranks,
                           std::initializer_list<Rank> blocked_ranks) {
    eval::TaperedScore score;
    for (const Rank rank : shelter_ranks)
        score += eval::pawn_shelter[rank];
    for (const Rank rank : storm_ranks)
        score += eval::pawn_storm[0][rank];
    for (const Rank rank : blocked_ranks)
        score += eval::pawn_storm[1][rank];
    return score;
}

eval::TaperedScore danger_score(int raw_danger) {
    return {raw_danger * raw_danger / 2048, raw_danger / 8};
}

} // namespace

TEST(EvaluationTest, SymmetricPositionsEqualTempoBonus) {
    const std::array<std::string_view, 2> fens = {
        board_test::fen::kings_only,
        board_test::fen::start,
    };

    for (const std::string_view fen : fens)
        EXPECT_EQ(eval::evaluate(Board(fen)), eval::tempo_bonus) << fen;
}

TEST(EvaluationTest, NullMoveOnlyChangesPerspectiveAndTempo) {
    Board board(board_test::fen::white_pawn_e2);

    const int white_eval = eval::evaluate(board);
    board.make_null();
    const int black_eval = eval::evaluate(board);

    EXPECT_GT(white_eval, eval::tempo_bonus);
    EXPECT_LT(black_eval, eval::tempo_bonus);
    EXPECT_EQ(white_eval + black_eval, 2 * eval::tempo_bonus);
}

TEST(EvaluationTermsTest, Mobility) {
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

    for (const auto& [fen, expected] : test_cases)
        expect_term_score(fen, eval::Term::Mobility, expected, expected);
}

TEST(EvaluationTermsTest, PinnedPieceMobilityStaysOnPinRay) {
    const Board               board(board_test::fen::pinned_rook);
    const eval::FeatureRecord record = eval::extract_features(board);

    EXPECT_EQ(record.coefficients[eval::feature::mobility(ROOK, 6)], 1);
    EXPECT_EQ(record.coefficients[eval::feature::mobility(ROOK, 12)], -1);
    EXPECT_EQ(record.coefficients[eval::feature::mobility(ROOK, 7)], 0);
    EXPECT_EQ(record.coefficients[eval::feature::mobility(ROOK, 13)], 0);
    EXPECT_EQ(record.term(eval::Term::Mobility).white, eval::rook_mob[6]);
    EXPECT_EQ(record.term(eval::Term::Mobility).black, eval::rook_mob[12]);
}

TEST(EvaluationTermsTest, Pawns) {
    constexpr auto both_isolated  = "4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1";
    constexpr auto white_isolated = "rnbqkbnr/ppppp1pp/8/8/8/8/P1PPPPPP/RNBQKBNR w KQkq - 0 2";
    constexpr auto black_isolated = "rnbqkbnr/pppppp1p/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 3";
    constexpr auto both_backward  = "4k3/8/3p4/2p5/2P5/1P6/8/4K3 w - - 0 4";
    constexpr auto white_backward = "4k3/8/8/2pp4/2P5/1P6/8/4K3 w - - 0 5";
    constexpr auto black_backward = "4k3/8/3p4/2p5/1PP5/8/8/4K3 w - - 0 6";
    constexpr auto white_doubled  = "4k3/5pp1/4p3/3p4/3PP3/4P3/5PP1/4K3 w - - 0 7";
    constexpr auto black_doubled  = "4k3/5pp1/4p3/3pp3/3P4/4P3/5PP1/4K3 w - - 0 8";
    constexpr auto white_isolated_doubled = "k7/8/8/8/8/P7/P7/K7 w KQkq - 0 9";
    constexpr auto opposed_pawns          = "4k3/8/2p5/8/1P6/8/8/4K3 w - - 0 1";
    constexpr auto passed_pawns           = "4k3/8/8/6p1/1P6/8/8/4K3 w - - 0 1";
    constexpr auto advanced_passers       = "4k3/P7/8/8/8/8/7p/4K3 w - - 0 1";
    constexpr auto isolated_not_backward  = "4k3/8/3p4/8/2P5/8/8/4K3 w - - 0 1";

    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore>> test_cases = {
        // sanity check
        {board_test::fen::kings_only, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        {board_test::fen::start, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        // isolated pawns
        {both_isolated, eval::isolated_pawn, eval::isolated_pawn},
        {white_isolated, eval::isolated_pawn, eval::TaperedScore::Zero},
        {black_isolated, eval::TaperedScore::Zero, eval::isolated_pawn},
        // backwards pawns
        {both_backward, eval::backward_pawn, eval::backward_pawn},
        {white_backward, eval::backward_pawn, eval::TaperedScore::Zero},
        {black_backward, eval::TaperedScore::Zero, eval::backward_pawn},
        // isolated pawns are not also backwards
        {isolated_not_backward, eval::isolated_pawn, eval::isolated_pawn},
        // doubled pawns
        {white_doubled, eval::doubled_pawn, eval::TaperedScore::Zero},
        {black_doubled, eval::TaperedScore::Zero, eval::doubled_pawn},
        // isolated and doubled pawns
        {white_isolated_doubled,
         eval::isolated_pawn * 2 + eval::doubled_pawn + eval::passed_pawn[RANK2]
             + eval::passed_pawn[RANK3],
         eval::TaperedScore::Zero},
        // passed pawns: opposed adjacent files, rank-four passers, advanced passers
        {opposed_pawns, eval::isolated_pawn, eval::isolated_pawn},
        {passed_pawns,
         eval::isolated_pawn + eval::passed_pawn[RANK4],
         eval::isolated_pawn + eval::passed_pawn[RANK4]},
        {advanced_passers,
         eval::isolated_pawn + eval::passed_pawn[RANK7],
         eval::isolated_pawn + eval::passed_pawn[RANK7]},
    };

    for (const auto& [fen, white, black] : test_cases)
        expect_term_score(fen, eval::Term::Pawns, white, black);
}

TEST(EvaluationTermsTest, Knights) {
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

    for (const auto& [fen, white, black] : test_cases)
        expect_term_score(fen, eval::Term::Knights, white, black);
}

TEST(EvaluationTermsTest, Bishops) {
    const auto start_score =
        eval::minor_pawn_shield * 2 + eval::bishop_pair + eval::bishop_blockers * 8;
    const auto has_outpost           = eval::bishop_outpost + eval::bishop_blockers * 2;
    const auto no_outpost            = eval::bishop_blockers * 4;
    const auto has_long_diagonal     = eval::bishop_long_diagonal + eval::bishop_blockers;
    const auto no_long_diagonal      = eval::bishop_blockers * 2;
    const auto two_pawns_defended    = eval::bishop_blockers * 2 + eval::bishop_outpost;
    const auto two_pawns_one_blocked = eval::bishop_blockers * 4;
    const auto two_pawns_two_blocked = eval::bishop_blockers * 6;

    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore>> test_cases = {
        {board_test::fen::kings_only, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        {board_test::fen::start, start_score, start_score},
        // bishop outposts
        {"6k1/8/2p5/4pBp1/4P1P1/2P3b1/8/6K1 w - - 0 1", has_outpost, no_outpost},
        {"6k1/8/2p3B1/4p1p1/4PbP1/2P5/8/6K1 w - - 0 2", no_outpost, has_outpost},
        // bishop behind pawn
        {"6k1/8/4p3/8/8/4P3/4B3/6K1 w - - 0 3", eval::minor_pawn_shield, eval::TaperedScore::Zero},
        {"6k1/4b3/4p3/8/8/4P3/8/6K1 w - - 0 4", eval::TaperedScore::Zero, eval::minor_pawn_shield},
        // bishop on long diagonal
        {"6k1/6b1/8/3P4/3p4/8/6B1/6K1 w - - 0 5", has_long_diagonal, has_long_diagonal},
        {"6k1/6b1/8/4p3/4P3/8/6B1/6K1 w - - 0 6", no_long_diagonal, no_long_diagonal},
        // bishop pair
        {"5bk1/8/8/8/8/8/8/4BBK1 w - - 0 7", eval::bishop_pair, eval::TaperedScore::Zero},
        {"4bbk1/8/8/8/8/8/8/5BK1 w - - 0 8", eval::TaperedScore::Zero, eval::bishop_pair},
        // bishop/pawn penalty
        {"4k3/8/8/2BPp3/2bpP3/8/8/4K3 w - - 0 9",
         eval::TaperedScore::Zero,
         eval::TaperedScore::Zero},
        {"4k3/8/8/2bPp3/2BpP3/8/8/4K3 w - - 0 10", two_pawns_one_blocked, two_pawns_one_blocked},
        {"4k3/8/8/3PpB2/3pPb2/8/8/4K3 w - - 0 11", two_pawns_defended, two_pawns_defended},
        {"4k3/4b3/8/4p3/3pP3/3P4/4B3/4K3 w - - 0 12", two_pawns_two_blocked, two_pawns_two_blocked},
    };

    for (const auto& [fen, white, black] : test_cases)
        expect_term_score(fen, eval::Term::Bishops, white, black);
}

TEST(EvaluationTermsTest, Rooks) {
    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore>> test_cases = {
        {board_test::fen::start, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        {board_test::fen::kings_only, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        {"6kr/8/8/8/8/8/8/RK6 w - - 0 1", eval::rook_open_file[1], eval::rook_open_file[1]},
        {"6kr/p7/8/8/8/8/7P/RK6 w - - 0 2", eval::rook_open_file[0], eval::rook_open_file[0]},
        {"rn5k/8/8/p7/P7/8/8/RN5K w - - 0 3", eval::rook_closed_file, eval::rook_closed_file},
    };

    for (const auto& [fen, white, black] : test_cases)
        expect_term_score(fen, eval::Term::Rooks, white, black);
}

TEST(EvaluationTermsTest, Queens) {
    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore>> test_cases = {
        {board_test::fen::start, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        {board_test::fen::kings_only, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        // a direct attack is not a discovered attack
        {"3rk3/8/8/8/8/8/8/3QK3 w - - 0 1", eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        // bishop discovered attack
        {"3qk3/2P5/1P6/B7/b7/1p6/8/3QK3 w - - 0 1",
         eval::queen_discovered_attack,
         eval::TaperedScore::Zero},
        {"3qk3/8/1P6/B7/b7/1p6/2p5/3QK3 w - - 0 2",
         eval::TaperedScore::Zero,
         eval::queen_discovered_attack},
        // rook discovered attack
        {"RNNqk3/8/8/8/8/8/8/rn1QK3 w - - 0 3",
         eval::queen_discovered_attack,
         eval::TaperedScore::Zero},
        {"RN1qk3/8/8/8/8/8/8/rnnQK3 w - - 0 4",
         eval::TaperedScore::Zero,
         eval::queen_discovered_attack},
    };

    for (const auto& [fen, white, black] : test_cases)
        expect_term_score(fen, eval::Term::Queens, white, black);
}

TEST(EvaluationTermsTest, Threats) {
    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore>> test_cases = {
        {board_test::fen::kings_only, eval::TaperedScore::Zero, eval::TaperedScore::Zero},
        {"4k3/8/8/7b/8/8/r3N3/4K3 w - - 0 1", eval::weak_piece[KNIGHT], eval::TaperedScore::Zero},
        {"4k3/8/R3n3/8/7B/8/8/4K3 w - - 0 2", eval::TaperedScore::Zero, eval::weak_piece[KNIGHT]},
        {"4k3/8/8/2p5/3N4/2P5/8/4K3 w - - 0 1", eval::weak_piece[KNIGHT], eval::TaperedScore::Zero},
    };

    for (const auto& [fen, white, black] : test_cases)
        expect_term_score(fen, eval::Term::Threats, white, black);
}

TEST(EvaluationTermsTest, KingSafety) {
    const auto empty = shelter({RANK1, RANK1, RANK1}, {RANK1, RANK1, RANK1}, {})
                     + eval::king_file[FILE5] + eval::king_open_file[true][true];
    const auto start = shelter({RANK2, RANK2, RANK2}, {RANK7, RANK7, RANK7}, {})
                     + eval::king_file[FILE7] + eval::king_open_file[false][false];

    std::vector<std::tuple<std::string, eval::TaperedScore>> test_cases = {
        {board_test::fen::kings_only, empty},
        {board_test::fen::start, start},
    };

    for (const auto& [fen, expected] : test_cases)
        expect_term_score(fen, eval::Term::KingSafety, expected, expected);

    const Board with_rights("4k2r/8/8/8/8/8/5PPP/4K2R w K - 0 1");
    const Board without_rights("4k2r/8/8/8/8/8/5PPP/4K2R w - - 0 1");
    const auto  current_shelter = shelter({RANK1, RANK1, RANK2}, {RANK1, RANK1, RANK1}, {})
                               + eval::king_file[FILE5] + eval::king_open_file[true][true];
    const auto castled_shelter = shelter({RANK2, RANK2, RANK2}, {RANK1, RANK1, RANK1}, {})
                               + eval::king_file[FILE7] + eval::king_open_file[false][true];
    const auto danger = danger_score(eval::unsafe_check_danger[ROOK]);

    const auto with_rights_score =
        eval::extract_features(with_rights).term(eval::Term::KingSafety).white;
    const auto without_rights_score =
        eval::extract_features(without_rights).term(eval::Term::KingSafety).white;

    EXPECT_EQ(with_rights_score, castled_shelter - danger);
    EXPECT_EQ(without_rights_score, current_shelter - danger);
    EXPECT_EQ(with_rights_score - without_rights_score, castled_shelter - current_shelter);
}

TEST(EvaluationTermsTest, Shelter) {
    const auto blocked = shelter({RANK3, RANK4, RANK5}, {RANK6, RANK4}, {RANK5})
                       + eval::king_file[FILE1] + eval::king_open_file[false][false];
    const auto own_pawn_only = shelter({RANK2, RANK2, RANK2}, {RANK1, RANK1, RANK1}, {})
                             + eval::king_file[FILE1] + eval::king_open_file[false][true];
    const auto opponent_pawn_only = shelter({RANK1, RANK1, RANK1}, {RANK7, RANK7, RANK7}, {})
                                  + eval::king_file[FILE1] + eval::king_open_file[true][false];
    const auto rank2 = shelter({RANK1, RANK1, RANK3}, {RANK7, RANK7, RANK6}, {})
                     + eval::king_file[FILE2] + eval::king_open_file[false][false];
    const auto attacked = shelter({RANK2, RANK2, RANK1}, {RANK7, RANK7, RANK7}, {})
                        + eval::king_file[FILE1] + eval::king_open_file[false][false];

    const auto weak_zone_danger = danger_score(eval::weak_king_zone_danger);
    std::vector<std::tuple<std::string, eval::TaperedScore, eval::TaperedScore>> test_cases = {
        {"k7/8/p7/1pP5/1Pp5/P7/8/K7 w - - 0 1",
         blocked - weak_zone_danger,
         blocked - weak_zone_danger},
        {"7k/5ppp/8/8/8/8/PPP5/K7 w - - 0 2", own_pawn_only, own_pawn_only},
        {"k7/5ppp/8/8/8/8/PPP5/7K w - - 0 3", opponent_pawn_only, opponent_pawn_only},
        {"8/5pkp/6p1/8/8/6P1/5PKP/8 w - - 0 4", rank2, rank2},
        {"k7/ppp5/3P4/8/8/3p4/PPP5/K7 w - - 0 5",
         attacked - weak_zone_danger,
         attacked - weak_zone_danger},
    };

    for (const auto& [fen, white, black] : test_cases)
        expect_term_score(fen, eval::Term::KingSafety, white, black);
}

TEST(EvaluationTermsTest, KingDanger) {
    const auto empty_e = shelter({RANK1, RANK1, RANK1}, {RANK1, RANK1, RANK1}, {})
                       + eval::king_file[FILE5] + eval::king_open_file[true][true];
    const auto empty_h = shelter({RANK1, RANK1, RANK1}, {RANK1, RANK1, RANK1}, {})
                       + eval::king_file[FILE8] + eval::king_open_file[true][true];
    const auto own_e2 = shelter({RANK1, RANK2, RANK1}, {RANK1, RANK1, RANK1}, {})
                      + eval::king_file[FILE5] + eval::king_open_file[false][true];
    const auto storm_e7 = shelter({RANK1, RANK1, RANK1}, {RANK1, RANK7, RANK1}, {})
                        + eval::king_file[FILE5] + eval::king_open_file[true][false];
    const int checks = eval::safe_check_danger[QUEEN] + eval::safe_check_danger[BISHOP]
                     + eval::king_zone_attack_danger[QUEEN] + eval::weak_king_zone_danger;
    const int pinned_defender = eval::unsafe_check_danger[ROOK]
                              + eval::king_zone_attack_danger[ROOK]
                              + 2 * eval::weak_king_zone_danger;

    using Case = std::tuple<std::string, eval::TaperedScore, eval::TaperedScore, int, int>;
    const std::vector<Case> test_cases = {
        {"4k3/5n2/8/8/8/8/4P3/4K1NR w - - 0 2",
         own_e2,
         storm_e7,
         0,
         eval::unsafe_check_danger[ROOK]},
        {"4k1nr/4p3/8/8/8/8/5N2/4K3 w - - 0 3",
         storm_e7,
         own_e2,
         eval::unsafe_check_danger[ROOK],
         0},
        {"4r2k/8/8/8/8/8/4n3/4K3 w - - 0 1", empty_e, empty_h, pinned_defender, 0},
        {"r1n1kn1r/8/8/8/8/8/8/R2QKB2 w - - 0 4", empty_e, empty_e, 0, checks},
        {"r2qkb2/8/8/8/8/8/8/R1N1KN1R w - - 0 5", empty_e, empty_e, checks, 0},
    };

    for (const auto& [fen, white_shelter, black_shelter, white_danger, black_danger] : test_cases) {
        expect_term_score(fen,
                          eval::Term::KingSafety,
                          white_shelter - danger_score(white_danger),
                          black_shelter - danger_score(black_danger));
    }
}

TEST(EvaluationTermsTest, EndgameScalingUsesStrongerSidePawnCount) {
    const std::vector<std::pair<std::string_view, int>> test_cases = {
        {"4k3/8/8/8/8/8/8/R3K3 w - - 0 1", eval::scale_base},
        {board_test::fen::white_pawn_e2, eval::scale_base + eval::scale_per_pawn},
        {"4k3/4p3/8/8/8/8/8/4K3 w - - 0 1", eval::scale_base + eval::scale_per_pawn},
        {"4k3/8/8/8/8/8/PPPP4/4K3 w - - 0 1", eval::scale_limit},
    };

    for (const auto& [fen, factor] : test_cases) {
        const eval::FeatureRecord record = eval::extract_features(Board(fen));
        SCOPED_TRACE(fen);
        ASSERT_NE(record.unscaled_score.eg, 0);
        EXPECT_EQ(record.scaled_score.mg, record.unscaled_score.mg);
        EXPECT_EQ(record.scaled_score.eg, record.unscaled_score.eg * factor / eval::scale_limit);
    }
}

TEST(EvaluationTermsTest, TaperingUsesMaterialPhase) {
    struct Case {
        std::string_view   fen;
        std::array<int, 4> phase_counts;
        int                phase;
    };

    const int               mixed_material = 4 * eval::rook.mg + eval::knight.mg + eval::bishop.mg;
    const int               rook_endgame   = 2 * eval::rook.mg;
    const std::vector<Case> test_cases     = {
        {board_test::fen::white_pawn_e2, {0, 0, 0, 0}, 0},
        {"rnbqkbnr/pppppppp/8/8/8/8/1PPPPPPP/QNBQKBNR w - - 0 1", {4, 4, 3, 3}, eval::phase_limit},
        {"krrnBRRK/8/8/8/8/8/8/8 w - - 0 1",
             {1, 1, 4, 0},
             mixed_material * eval::phase_limit / eval::material_mg},
        {"kr6/8/8/8/8/8/8/5R1K w - - 0 1",
             {0, 0, 2, 0},
             rook_endgame * eval::phase_limit / eval::material_mg},
    };

    for (const auto& [fen, phase_counts, phase] : test_cases) {
        const eval::FeatureRecord record = eval::extract_features(Board(fen));
        SCOPED_TRACE(fen);
        ASSERT_NE(record.scaled_score.mg, record.scaled_score.eg);
        EXPECT_EQ(record.phase_counts, phase_counts);
        EXPECT_EQ(
            record.tapered_value,
            (record.scaled_score.mg * phase + record.scaled_score.eg * (eval::phase_limit - phase))
                / eval::phase_limit);
    }
}
