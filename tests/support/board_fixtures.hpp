#pragma once

#include "board/board.hpp"

namespace board_test::fen {

inline constexpr auto& start = Board::startfen;

inline constexpr char perft_position_2[] =
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
inline constexpr char perft_position_3[] = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
inline constexpr char perft_position_4_white[] =
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
inline constexpr char perft_position_4_black[] =
    "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1";
inline constexpr char perft_position_5[] =
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
inline constexpr char perft_position_6[] =
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";

inline constexpr char kings_only[]          = "4k3/8/8/8/8/8/8/4K3 w - - 0 1";
inline constexpr char corner_kings[]        = "7k/8/8/8/8/8/8/K7 w - - 0 1";
inline constexpr char quiet_black_to_move[] = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
inline constexpr char stalemate[]           = "k7/8/KQ6/8/8/8/8/8 b - - 0 1";
inline constexpr char one_legal_evasion[]   = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
inline constexpr char white_pawn_e2[]       = "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1";
inline constexpr char after_e2e4[] = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
inline constexpr char legal_en_passant_a3[]       = "4k3/8/8/8/Pp6/8/8/4K3 b - a3 0 1";
inline constexpr char unhashable_en_passant_e3[]  = "4k3/8/8/8/4P3/8/8/4K3 b - e3 0 1";
inline constexpr char en_passant_d6_with_clocks[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6 10 20";
inline constexpr char pinned_en_passant_e3[]      = "8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - e3 0 1";

inline constexpr char promotion_options[] = "4k3/P6p/8/8/8/8/p6P/4K3 w - - 0 1";
inline constexpr char capture_promotion[] = "1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1";
inline constexpr char white_pawn_on_a7[]  = "4k3/P7/8/8/8/8/8/4K3 w - - 0 1";
inline constexpr char castling[]          = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
inline constexpr char check_evasion[]     = "k3r3/8/8/8/8/8/8/2B1K1N1 w - - 0 1";
inline constexpr char multiple_pinners[]  = "4r2k/4q3/8/8/8/8/4N3/4K3 w - - 0 1";
inline constexpr char repetition_cycle[] =
    "3r4/ppq4k/1nb1BQ1p/4Pp1p/1b6/8/PP3PPP/2R1R1K1 w - - 2 26";
inline constexpr char pinned_rook[]                = "k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1";
inline constexpr char checking_move_candidates[]   = "4k3/8/8/8/6N1/8/8/RB1QK3 w - - 0 1";
inline constexpr char max_halfmove_long_fullmove[] = "4k3/8/8/8/8/8/8/4K3 w - - 255 300";

} // namespace board_test::fen
