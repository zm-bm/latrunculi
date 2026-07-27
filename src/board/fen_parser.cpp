#include "board/fen_parser.hpp"

#include "core/notation.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

// Legal play never increases the initial 32-piece population.
constexpr std::size_t max_position_pieces = 32;

bool is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

int parse_int(std::string_view field, const char* name) {
    if (field.empty())
        throw std::invalid_argument(std::string("invalid fen, missing ") + name);

    for (char ch : field) {
        if (!is_digit(ch))
            throw std::invalid_argument(std::string("invalid fen, invalid ") + name);
    }

    int value            = 0;
    const auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), value);
    if (ec != std::errc{} || ptr != field.data() + field.size())
        throw std::invalid_argument(std::string("invalid fen, invalid ") + name);

    return value;
}

std::uint8_t parse_uint8(std::string_view field, const char* name) {
    const int value = parse_int(field, name);
    if (value > std::numeric_limits<std::uint8_t>::max())
        throw std::invalid_argument(std::string("invalid fen, invalid ") + name);
    return static_cast<std::uint8_t>(value);
}

int fullmove_to_absolute_ply(int fullmove_number, Color side_to_move) {
    const long long value = 2LL * (fullmove_number - 1) + (side_to_move == WHITE ? 0 : 1);
    if (value > std::numeric_limits<int>::max())
        throw std::invalid_argument("invalid fen, fullmove number is too large");
    return static_cast<int>(value);
}

PieceSquare parse_piece(char symbol, Square square) {
    switch (symbol) {
    case 'P': return {WHITE, PAWN, square};
    case 'N': return {WHITE, KNIGHT, square};
    case 'B': return {WHITE, BISHOP, square};
    case 'R': return {WHITE, ROOK, square};
    case 'Q': return {WHITE, QUEEN, square};
    case 'K': return {WHITE, KING, square};
    case 'p': return {BLACK, PAWN, square};
    case 'n': return {BLACK, KNIGHT, square};
    case 'b': return {BLACK, BISHOP, square};
    case 'r': return {BLACK, ROOK, square};
    case 'q': return {BLACK, QUEEN, square};
    case 'k': return {BLACK, KING, square};
    default:  throw std::invalid_argument("invalid fen, invalid piece placement");
    }
}

void parse_piece_placement(ParsedFen& parsed, std::string_view field) {
    int white_kings = 0;
    int black_kings = 0;
    int file        = 0;
    int rank        = 7;

    parsed.pieces.reserve(max_position_pieces);
    for (char ch : field) {
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

            const PieceSquare piece = parse_piece(ch, square::make(File(file), Rank(rank)));
            if (piece.type == PAWN && (rank == 0 || rank == 7))
                throw std::invalid_argument("invalid fen, invalid pawn placement");
            if (piece.type == KING)
                (piece.color == WHITE ? white_kings : black_kings)++;
            if (parsed.pieces.size() == max_position_pieces)
                throw std::invalid_argument("invalid fen, too many pieces");

            parsed.pieces.emplace_back(piece);
            ++file;
        }
    }

    if (rank != 0 || file != 8 || white_kings != 1 || black_kings != 1)
        throw std::invalid_argument("invalid fen, invalid piece placement");
}

void parse_side_to_move(ParsedFen& parsed, std::string_view field) {
    if (field == "w")
        parsed.turn = WHITE;
    else if (field == "b")
        parsed.turn = BLACK;
    else
        throw std::invalid_argument("invalid fen, invalid side to move");
}

void parse_castling_rights(ParsedFen& parsed, std::string_view field) {
    if (field == "-")
        return;
    if (field.empty())
        throw std::invalid_argument("invalid fen, invalid castling rights");

    constexpr std::string_view order = "KQkq";
    size_t                     next  = 0;

    for (char ch : field) {
        const size_t pos = order.find(ch, next);
        if (pos == std::string_view::npos)
            throw std::invalid_argument("invalid fen, invalid castling rights");
        next = pos + 1;

        switch (ch) {
        case 'K': parsed.castling_rights |= W_KINGSIDE; break;
        case 'Q': parsed.castling_rights |= W_QUEENSIDE; break;
        case 'k': parsed.castling_rights |= B_KINGSIDE; break;
        case 'q': parsed.castling_rights |= B_QUEENSIDE; break;
        default:  break;
        }
    }
}

void parse_enpassant_target(ParsedFen& parsed, std::string_view field) {
    if (field == "-")
        return;
    if (field.size() != 2 || field[0] < 'a' || field[0] > 'h')
        throw std::invalid_argument("invalid fen, invalid en passant square");

    const char expected_rank = (parsed.turn == WHITE) ? '6' : '3';
    if (field[1] != expected_rank)
        throw std::invalid_argument("invalid fen, invalid en passant square");

    parsed.enpassant_target = parse_square(field);
}

} // namespace

ParsedFen parse_fen(std::string_view fen) {
    std::istringstream       input{std::string(fen)};
    std::vector<std::string> fields;
    std::string              field;

    while (input >> field)
        fields.push_back(field);

    if (fields.size() != 4 && fields.size() != 6)
        throw std::invalid_argument("invalid fen, must have 4 or 6 fields");

    ParsedFen parsed;
    parse_piece_placement(parsed, fields[0]);
    parse_side_to_move(parsed, fields[1]);
    parse_castling_rights(parsed, fields[2]);
    parse_enpassant_target(parsed, fields[3]);

    if (fields.size() == 6) {
        parsed.halfmove_clock     = parse_uint8(fields[4], "halfmove clock");
        const int fullmove_number = parse_int(fields[5], "fullmove number");
        if (fullmove_number == 0)
            throw std::invalid_argument("invalid fen, invalid fullmove number");
        parsed.absolute_ply = fullmove_to_absolute_ply(fullmove_number, parsed.turn);
    } else {
        parsed.absolute_ply = fullmove_to_absolute_ply(1, parsed.turn);
    }

    return parsed;
}
