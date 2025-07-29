#pragma once

#include <string>
#include <vector>

#include "bb.hpp"

#include "defs.hpp"
#include "eval.hpp"
#include "move.hpp"
#include "score.hpp"
#include "state.hpp"
#include "zobrist.hpp"

class Thread;

class Board {
private:
    uint64_t piece_bb[N_COLORS][N_PIECES]     = {0};
    uint8_t  piece_counts[N_COLORS][N_PIECES] = {0};
    Piece    squares[N_SQUARES]               = {NO_PIECE};
    Square   king_square[N_COLORS]            = {E1, E8};
    Color    turn                             = WHITE;

    uint32_t ply          = 0;
    uint32_t fullmove_clk = 0;

    Score material  = {0, 0};
    Score psq_bonus = {0, 0};

    std::vector<State> state  = {State()};
    Thread*            thread = nullptr;

public:
    explicit Board(const std::string& fen);
    Board()                        = delete;
    Board(const Board&)            = delete;
    Board& operator=(const Board&) = delete;

    void load_board(const Board*);
    void load_fen(const std::string&);
    void set_thread(Thread* t) { thread = t; }
    void reset();

    // accessors

    template <PieceType... Ps>
    uint64_t pieces() const;
    template <PieceType... Ps>
    uint64_t pieces(Color c) const;

    uint64_t  occupancy() const { return pieces<ALL_PIECES>(); }
    uint8_t   count(Color c, PieceType p) const { return piece_counts[c][p]; }
    Piece     piece_on(Square sq) const { return squares[sq]; }
    Piece     piece_on(File f, Rank r) const { return squares[make_square(f, r)]; };
    PieceType piecetype_on(Square sq) const { return type_of(squares[sq]); }
    Square    king_sq(Color c) const { return king_square[c]; }
    Color     side_to_move() const { return turn; }
    Score     material_score() const { return material; }
    Score     psq_bonus_score() const { return psq_bonus; }
    uint8_t   fullmove() const { return (fullmove_clk / 2) + 1; }

    CastleRights castle_rights() const { return state.at(ply).castle; }
    uint64_t     checkers() const { return state.at(ply).checkers; }
    uint64_t     blockers(Color c) const { return state.at(ply).blockers[c]; }
    Square       enpassant_sq() const { return state.at(ply).enpassant; }
    uint8_t      halfmove() const { return state.at(ply).halfmove_clk; }

    State& get_state() { return state.at(ply); }

    // castling

    bool can_castle(Color c) const;
    bool can_castle_kingside(Color c) const;
    bool can_castle_queenside(Color c) const;
    void disable_castle(Color c);
    void disable_castle(Color c, Square sq);

    // attack bitboards

    uint64_t attacks_to(Square sq, Color c, uint64_t occupancy) const;
    uint64_t attacks_to(Square sq, uint64_t occupancy) const;
    uint64_t attacks_to(Square sq) const { return attacks_to(sq, occupancy()); }
    uint64_t attacks_to(Square sq, Color c) const { return attacks_to(sq, c, occupancy()); }
    uint64_t attacks_to(uint64_t bitboard, Color c) const;

    // piece modifiers

    template <bool>
    void add_piece(Square, Color, PieceType);
    template <bool>
    void remove_piece(Square, Color, PieceType);
    template <bool>
    void move_piece(Square, Square, Color, PieceType);

    // check data update methods

    void update_check_data();
    void update_pinners_and_blockers(Color);

    // move properties

    bool is_capture(Move move) const { return piecetype_on(move.to()) != NO_PIECETYPE; }
    bool is_legal_move(Move move) const;
    bool is_checking_move(Move move) const;
    int  seeMove(Move move) const;

    // make moves

    void make(Move);
    void unmake();
    void make_null();
    void unmake_null();

    // zobrist keys

    uint64_t key() const { return state.at(ply).zkey; }
    uint64_t calculate_key() const;

    // checks and draws

    bool is_check() const { return checkers(); }
    bool is_double_check() const { return bb::is_many(checkers()); };
    bool is_stalemate() const;
    bool is_draw() const;

    // string conversions

    std::string toFEN() const;
    std::string toSAN(Move) const;

    // perft

    template <NodeType = ROOT>
    uint64_t perft(int, std::ostream& = std::cout);

    // eval helpers

    int nonPawnMaterial(Color c) const;

    static constexpr char startfen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
};

inline Board::Board(const std::string& fen) {
    load_fen(fen);
}

inline void Board::load_board(const Board* other) {
    if (!other)
        return;

    load_fen(other->toFEN());
    state = other->state;
    ply   = other->ply;
}

template <PieceType... Ps>
inline uint64_t Board::pieces() const {
    return ((piece_bb[WHITE][Ps] | piece_bb[BLACK][Ps]) | ...);
}

template <PieceType... Ps>
inline uint64_t Board::pieces(Color c) const {
    return (piece_bb[c][Ps] | ...);
};

inline bool Board::can_castle(Color c) const {
    return castle_rights() & (c ? W_CASTLE : B_CASTLE);
};

inline bool Board::can_castle_kingside(Color c) const {
    return castle_rights() & (c ? W_KINGSIDE : B_KINGSIDE);
};

inline bool Board::can_castle_queenside(Color c) const {
    return castle_rights() & (c ? W_QUEENSIDE : B_QUEENSIDE);
};

inline void Board::disable_castle(Color c) {
    auto& st = get_state();

    if (can_castle_kingside(c))
        st.zkey ^= zob::castle[CASTLE_KINGSIDE][c];
    if (can_castle_queenside(c))
        st.zkey ^= zob::castle[CASTLE_QUEENSIDE][c];

    st.castle &= (c == WHITE ? B_CASTLE : W_CASTLE);
}

inline void Board::disable_castle(Color c, Square sq) {
    auto& st = get_state();

    if (sq == castle::rook_from[CASTLE_KINGSIDE][c] && can_castle_kingside(c)) {
        st.zkey   ^= zob::castle[CASTLE_KINGSIDE][c];
        st.castle &= ~(c == WHITE ? W_KINGSIDE : B_KINGSIDE);
    }

    else if (sq == castle::rook_from[CASTLE_QUEENSIDE][c] && can_castle_queenside(c)) {
        st.zkey   ^= zob::castle[CASTLE_QUEENSIDE][c];
        st.castle &= ~(c == WHITE ? W_QUEENSIDE : B_QUEENSIDE);
    }
}

// Returns a bitboard of pieces of color c which attack a square
inline uint64_t Board::attacks_to(Square sq, Color c, uint64_t occupied) const {
    return (pieces<PAWN>(c) & bb::pawn_attacks(bb::set(sq), ~c)) |
           (pieces<KNIGHT>(c) & bb::moves<KNIGHT>(sq, occupied)) |
           (pieces<KING>(c) & bb::moves<KING>(sq, occupied)) |
           (pieces<BISHOP, QUEEN>(c) & bb::moves<BISHOP>(sq, occupied)) |
           (pieces<ROOK, QUEEN>(c) & bb::moves<ROOK>(sq, occupied));
}

// Returns a bitboard of pieces of any color which attack a square
inline uint64_t Board::attacks_to(Square sq, uint64_t occupied) const {
    return (pieces<PAWN>(WHITE) & bb::pawn_attacks<BLACK>(bb::set(sq))) |
           (pieces<PAWN>(BLACK) & bb::pawn_attacks<WHITE>(bb::set(sq))) |
           (pieces<KNIGHT>() & bb::moves<KNIGHT>(sq, occupied)) |
           (pieces<KING>() & bb::moves<KING>(sq, occupied)) |
           (pieces<BISHOP, QUEEN>() & bb::moves<BISHOP>(sq, occupied)) |
           (pieces<ROOK, QUEEN>() & bb::moves<ROOK>(sq, occupied));
}

// Determine if any bitboard is attacked by color c
inline uint64_t Board::attacks_to(uint64_t bitboard, Color c) const {
    uint64_t occ = occupancy();
    while (bitboard) {
        Square sq = bb::lsb_pop(bitboard);
        if (attacks_to(sq, c, occ))
            return true;
    }
    return false;
}

template <bool apply_hash>
inline void Board::add_piece(Square sq, Color c, PieceType pt) {
    piece_counts[c][pt]++;
    piece_bb[c][pt]         ^= bb::set(sq);
    piece_bb[c][ALL_PIECES] ^= bb::set(sq);
    squares[sq]              = make_piece(c, pt);
    material                += eval::piece(pt, c);
    psq_bonus               += eval::piece_sq(pt, c, sq);
    if constexpr (apply_hash)
        state.at(ply).zkey ^= zob::hash_piece(c, pt, sq);
}

template <bool apply_hash>
inline void Board::remove_piece(Square sq, Color c, PieceType pt) {
    piece_counts[c][pt]--;
    piece_bb[c][pt]         ^= bb::set(sq);
    piece_bb[c][ALL_PIECES] ^= bb::set(sq);
    squares[sq]              = NO_PIECE;
    material                -= eval::piece(pt, c);
    psq_bonus               -= eval::piece_sq(pt, c, sq);
    if constexpr (apply_hash)
        state.at(ply).zkey ^= zob::hash_piece(c, pt, sq);
}

template <bool apply_hash>
inline void Board::move_piece(Square from, Square to, Color c, PieceType pt) {
    uint64_t mask            = bb::set(from) | bb::set(to);
    piece_bb[c][pt]         ^= mask;
    piece_bb[c][ALL_PIECES] ^= mask;
    squares[from]            = NO_PIECE;
    squares[to]              = make_piece(c, pt);
    psq_bonus               += eval::piece_sq(pt, c, to) - eval::piece_sq(pt, c, from);
    if constexpr (apply_hash)
        state.at(ply).zkey ^= zob::hash_piece(c, pt, from) ^ zob::hash_piece(c, pt, to);
}

inline void Board::update_check_data() {
    Color    opp      = ~turn;
    Square   opp_king = king_sq(opp);
    uint64_t occ      = occupancy();

    state[ply].checkers       = attacks_to(king_sq(turn), opp);
    state[ply].checks[PAWN]   = bb::pawn_attacks(bb::set(opp_king), opp);
    state[ply].checks[KNIGHT] = bb::moves<KNIGHT>(opp_king, occ);
    state[ply].checks[BISHOP] = bb::moves<BISHOP>(opp_king, occ);
    state[ply].checks[ROOK]   = bb::moves<ROOK>(opp_king, occ);
    state[ply].checks[QUEEN]  = state[ply].checks[BISHOP] | state[ply].checks[ROOK];
    update_pinners_and_blockers(WHITE);
    update_pinners_and_blockers(BLACK);
}

// Update enemy pinning pieces and all blocking pieces for the king of a given color
inline void Board::update_pinners_and_blockers(Color c) {
    Color    opp     = ~c;
    Square   king    = king_sq(c);
    uint64_t occ     = occupancy();
    uint64_t sliders = (bb::moves<BISHOP>(king) & pieces<BISHOP, QUEEN>(opp)) |
                       (bb::moves<ROOK>(king) & pieces<ROOK, QUEEN>(opp));

    // For each potential pinner, check if only one piece is blocking the king
    while (sliders) {
        Square   pinner         = bb::lsb_pop(sliders);
        uint64_t pieces_between = occ & bb::between(king, pinner);

        if (!bb::is_many(pieces_between)) {
            state[ply].pinners[opp] |= bb::set(pinner);
            state[ply].blockers[c]  |= pieces_between & occ;
        }
    }
}

inline int Board::nonPawnMaterial(Color c) const {
    return ((count(c, KNIGHT) * KNIGHT_MG) + (count(c, BISHOP) * BISHOP_MG) +
            (count(c, ROOK) * ROOK_MG) + (count(c, QUEEN) * QUEEN_MG));
}

template <>
struct std::formatter<Board> : std::formatter<std::string_view> {
    auto format(const Board& b, std::format_context& ctx) const {
        auto out = ctx.out();

        for (Rank rank = RANK8; rank >= RANK1; --rank) {
            out = std::format_to(out, "   +---+---+---+---+---+---+---+---+\n");
            out = std::format_to(out, "   |");
            for (File file = FILE1; file <= FILE8; ++file) {
                out = std::format_to(out, " {} |", to_char(b.piece_on(file, rank)));
            }
            out = std::format_to(out, " {}\n", to_char(rank));
        };
        out = std::format_to(out, "   +---+---+---+---+---+---+---+---+\n");
        out = std::format_to(out, "     a   b   c   d   e   f   g   h\n\n");
        out = std::format_to(out, "FEN: {}\n", b.toFEN());

        return out;
    }
};
