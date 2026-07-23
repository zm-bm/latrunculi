#include "board/fen.hpp"

#include "core/notation.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

bool is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

int parse_int(std::string_view section, const char* name) {
    if (section.empty())
        throw std::invalid_argument(std::string("invalid fen, missing ") + name);

    for (char ch : section) {
        if (!is_digit(ch))
            throw std::invalid_argument(std::string("invalid fen, invalid ") + name);
    }

    int value            = 0;
    const auto [ptr, ec] = std::from_chars(section.data(), section.data() + section.size(), value);
    if (ec != std::errc{} || ptr != section.data() + section.size())
        throw std::invalid_argument(std::string("invalid fen, invalid ") + name);

    return value;
}

std::uint8_t parse_uint8(std::string_view section, const char* name) {
    const int value = parse_int(section, name);
    if (value > std::numeric_limits<std::uint8_t>::max())
        throw std::invalid_argument(std::string("invalid fen, invalid ") + name);
    return static_cast<std::uint8_t>(value);
}

int fen_fullmove_to_ply(int fullmove_num, Color turn) {
    const long long value = 2LL * (fullmove_num - 1) + (turn == WHITE ? 0 : 1);
    if (value > std::numeric_limits<int>::max())
        throw std::invalid_argument("invalid fen, fullmove number is too large");
    return static_cast<int>(value);
}

PieceSquare make_piece_square(char ch, Square sq) {
    switch (ch) {
    case 'P': return {WHITE, PAWN, sq};
    case 'N': return {WHITE, KNIGHT, sq};
    case 'B': return {WHITE, BISHOP, sq};
    case 'R': return {WHITE, ROOK, sq};
    case 'Q': return {WHITE, QUEEN, sq};
    case 'K': return {WHITE, KING, sq};
    case 'p': return {BLACK, PAWN, sq};
    case 'n': return {BLACK, KNIGHT, sq};
    case 'b': return {BLACK, BISHOP, sq};
    case 'r': return {BLACK, ROOK, sq};
    case 'q': return {BLACK, QUEEN, sq};
    case 'k': return {BLACK, KING, sq};
    default:  throw std::invalid_argument("invalid fen, invalid piece placement");
    }
}

void parse_pieces(ParsedFen& parsed, std::string_view section) {
    int white_kings = 0;
    int black_kings = 0;
    int file        = 0;
    int rank        = 7;

    parsed.pieces.reserve(32);
    for (char ch : section) {
        if (is_digit(ch)) {
            if (ch == '0')
                throw std::invalid_argument("invalid fen, invalid piece placement");
            file += int(ch - '0');
            if (file > 8)
                throw std::invalid_argument("invalid fen, invalid piece placement");
        } else if (ch == '/') {
            if (file != 8 || rank == 0)
                throw std::invalid_argument("invalid fen, invalid piece placement");
            --rank;
            file = 0;
        } else {
            if (file >= 8)
                throw std::invalid_argument("invalid fen, invalid piece placement");

            auto piece = make_piece_square(ch, square::make(File(file), Rank(rank)));
            if (piece.type == PAWN && (rank == 0 || rank == 7))
                throw std::invalid_argument("invalid fen, invalid pawn placement");
            if (piece.type == KING)
                (piece.color == WHITE ? white_kings : black_kings)++;

            parsed.pieces.emplace_back(piece);
            ++file;
        }
    }

    if (rank != 0 || file != 8 || white_kings != 1 || black_kings != 1)
        throw std::invalid_argument("invalid fen, invalid piece placement");
}

void parse_turn(ParsedFen& parsed, std::string_view section) {
    if (section == "w")
        parsed.turn = WHITE;
    else if (section == "b")
        parsed.turn = BLACK;
    else
        throw std::invalid_argument("invalid fen, invalid side to move");
}

void parse_castles(ParsedFen& parsed, std::string_view section) {
    if (section == "-")
        return;
    if (section.empty())
        throw std::invalid_argument("invalid fen, invalid castling rights");

    constexpr std::string_view order = "KQkq";
    size_t                     next  = 0;

    for (char ch : section) {
        const size_t pos = order.find(ch, next);
        if (pos == std::string_view::npos)
            throw std::invalid_argument("invalid fen, invalid castling rights");
        next = pos + 1;

        switch (ch) {
        case 'K': parsed.castle |= W_KINGSIDE; break;
        case 'Q': parsed.castle |= W_QUEENSIDE; break;
        case 'k': parsed.castle |= B_KINGSIDE; break;
        case 'q': parsed.castle |= B_QUEENSIDE; break;
        default:  break;
        }
    }
}

void parse_enpassant(ParsedFen& parsed, std::string_view section) {
    if (section == "-")
        return;
    if (section.size() != 2 || section[0] < 'a' || section[0] > 'h')
        throw std::invalid_argument("invalid fen, invalid en passant square");

    const char expected_rank = (parsed.turn == WHITE) ? '6' : '3';
    if (section[1] != expected_rank)
        throw std::invalid_argument("invalid fen, invalid en passant square");

    parsed.enpassant_target = parse_square(section);
}

} // namespace

ParsedFen parse_fen(std::string_view fen) {
    std::istringstream       iss{std::string(fen)};
    std::vector<std::string> sections;
    std::string              section;

    while (iss >> section)
        sections.push_back(section);

    if (sections.size() != 4 && sections.size() != 6)
        throw std::invalid_argument("invalid fen, must have 4 or 6 sections");

    ParsedFen parsed;
    parse_pieces(parsed, sections[0]);
    parse_turn(parsed, sections[1]);
    parse_castles(parsed, sections[2]);
    parse_enpassant(parsed, sections[3]);

    if (sections.size() == 6) {
        parsed.halfmove_clk    = parse_uint8(sections[4], "halfmove clock");
        const int fullmove_num = parse_int(sections[5], "fullmove number");
        if (fullmove_num == 0)
            throw std::invalid_argument("invalid fen, invalid fullmove number");
        parsed.absolute_ply = fen_fullmove_to_ply(fullmove_num, parsed.turn);
    } else {
        parsed.absolute_ply = fen_fullmove_to_ply(1, parsed.turn);
    }

    return parsed;
}
