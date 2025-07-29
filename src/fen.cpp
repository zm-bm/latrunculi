#include "fen.hpp"
#include "util.hpp"
#include <sstream>

FenParser::FenParser(const std::string& fen) {
    std::istringstream iss(fen);
    std::string        pieces_str, color_str, castle_str, enpassant_str;

    if (!(iss >> pieces_str >> color_str >> castle_str >> enpassant_str)) {
        throw std::invalid_argument("invalid fen, must have at least 4 sections");
    }

    parse_pieces(pieces_str);
    parse_turn(color_str);
    parse_castles(castle_str);
    parse_enpassant(enpassant_str);

    std::string halfmove, fullmove;
    if (iss >> halfmove)
        parse_halfmove(halfmove);
    if (iss >> fullmove)
        parse_fullmove(fullmove, turn);
}

void FenParser::parse_pieces(const std::string& section) {
    auto file = FILE1;
    auto rank = RANK8;

    for (char ch : section) {

        if (std::isdigit(ch))
            file = file + int(ch - '0');

        else if (std::isalpha(ch)) {
            Square sq = make_square(file, rank);

            PieceSquare p = {.square = INVALID};
            switch (ch) {
            case 'P': p = {WHITE, PAWN, sq}; break;
            case 'N': p = {WHITE, KNIGHT, sq}; break;
            case 'B': p = {WHITE, BISHOP, sq}; break;
            case 'R': p = {WHITE, ROOK, sq}; break;
            case 'Q': p = {WHITE, QUEEN, sq}; break;
            case 'K': p = {WHITE, KING, sq}; break;
            case 'p': p = {BLACK, PAWN, sq}; break;
            case 'n': p = {BLACK, KNIGHT, sq}; break;
            case 'b': p = {BLACK, BISHOP, sq}; break;
            case 'r': p = {BLACK, ROOK, sq}; break;
            case 'q': p = {BLACK, QUEEN, sq}; break;
            case 'k': p = {BLACK, KING, sq}; break;
            default:  break;
            }

            if (p.square != INVALID)
                pieces.emplace_back(p);

            ++file;
        }

        else if (ch == '/') {
            --rank;
            file = FILE1;
        }
    }
}

void FenParser::parse_turn(const std::string& section) {
    turn = (section == "w") ? WHITE : BLACK;
}

void FenParser::parse_castles(const std::string& section) {
    if (section.find('K') != std::string::npos)
        castle |= W_KINGSIDE;
    if (section.find('Q') != std::string::npos)
        castle |= W_QUEENSIDE;
    if (section.find('k') != std::string::npos)
        castle |= B_KINGSIDE;
    if (section.find('q') != std::string::npos)
        castle |= B_QUEENSIDE;
}

void FenParser::parse_enpassant(const std::string& section) {
    if (section.compare("-") != 0) {
        enpassant = make_square(section);
    }
}

void FenParser::parse_halfmove(const std::string& section) {
    halfmove_clk = std::stoi(section);
}

void FenParser::parse_fullmove(const std::string& section, Color turn) {
    fullmove_clk = 2 * (std::stoul(section) - 1) + (turn == WHITE ? 0 : 1);
}
