#include "fen.hpp"

FenParser::FenParser(const std::string& fen) : fen(fen) { 
    parse();
}

void FenParser::parse() {
    std::vector<std::string> sections = splitStr(fen, ' ');

    if (sections.size() < 4) {
        throw std::invalid_argument("Invalid FEN: must have at least 4 sections");
    }

    parsePiecePlacement(sections.at(0));
    parseActiveColor(sections.at(1));
    parseCastlingRights(sections.at(2));
    parseEnPassantTarget(sections.at(3));
    if (sections.size() > 4) parseHalfmove(sections.at(4));
    if (sections.size() > 5)  parseFullmove(sections.at(5), active_color_);
}

std::vector<PieceSquare> FenParser::getPiecePlacement() const { return piece_placement_; }
Color FenParser::getActiveColor() const { return active_color_; }
CastleRights FenParser::getCastlingRights() const { return castling_rights_; }
Square FenParser::getEnPassantTarget() const { return en_passant_target_; }
U8 FenParser::getHalfmoveClock() const { return halfmove_clock_; }
U32 FenParser::getFullmoveNumber() const { return fullmove_number_; }

void FenParser::parsePiecePlacement(const std::string& section) {
    auto file = FILE1;
    auto rank = RANK8;

    for (auto iter = section.begin(); iter != section.end(); ++iter) {
        auto ch = *iter;

        if (((int)ch > 48) && ((int)ch < 57)) {
            file += File((int)ch - '0');
        } else {
            Square sq = makeSquare(file, rank);

            switch (ch) {
                case 'P':
                    piece_placement_.emplace_back(PieceSquare{WHITE, PAWN, sq});
                    file++;
                    break;
                case 'N':
                    piece_placement_.emplace_back(PieceSquare{WHITE, KNIGHT, sq});
                    file++;
                    break;
                case 'B':
                    piece_placement_.emplace_back(PieceSquare{WHITE, BISHOP, sq});
                    file++;
                    break;
                case 'R':
                    piece_placement_.emplace_back(PieceSquare{WHITE, ROOK, sq});
                    file++;
                    break;
                case 'Q':
                    piece_placement_.emplace_back(PieceSquare{WHITE, QUEEN, sq});
                    file++;
                    break;
                case 'K':
                    piece_placement_.emplace_back(PieceSquare{WHITE, KING, sq});
                    file++;
                    break;
                case 'p':
                    piece_placement_.emplace_back(PieceSquare{BLACK, PAWN, sq});
                    file++;
                    break;
                case 'n':
                    piece_placement_.emplace_back(PieceSquare{BLACK, KNIGHT, sq});
                    file++;
                    break;
                case 'b':
                    piece_placement_.emplace_back(PieceSquare{BLACK, BISHOP, sq});
                    file++;
                    break;
                case 'r':
                    piece_placement_.emplace_back(PieceSquare{BLACK, ROOK, sq});
                    file++;
                    break;
                case 'q':
                    piece_placement_.emplace_back(PieceSquare{BLACK, QUEEN, sq});
                    file++;
                    break;
                case 'k':
                    piece_placement_.emplace_back(PieceSquare{BLACK, KING, sq});
                    file++;
                    break;
                case '/':
                    rank--;
                    file = FILE1;
                    break;
            }
        }
    }
}

void FenParser::parseActiveColor(const std::string& section) {
    active_color_ = (section == "w") ? WHITE : BLACK;
}

void FenParser::parseCastlingRights(const std::string& section) {
    if (section.find('-') == std::string::npos) {
        if (section.find('K') != std::string::npos) {
            castling_rights_ |= WHITE_OO;
        }

        if (section.find('Q') != std::string::npos) {
            castling_rights_ |= WHITE_OOO;
        }

        if (section.find('k') != std::string::npos) {
            castling_rights_ |= BLACK_OO;
        }

        if (section.find('q') != std::string::npos) {
            castling_rights_ |= BLACK_OOO;
        }
    }
}

void FenParser::parseEnPassantTarget(const std::string& section) {
    if (section.compare("-") != 0) {
        en_passant_target_ = makeSquare(section);
    }
}

void FenParser::parseHalfmove(const std::string& section) {
    halfmove_clock_ = std::stoi(section);
}

void FenParser::parseFullmove(const std::string& section, Color turn) {
    fullmove_number_ = 2 * (std::stoul(section) - 1) + (turn == WHITE ? 0 : 1);
}
