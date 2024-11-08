#include "fen.hpp"

FenParser::FenParser(const std::string& fen) : fen(fen) { 
    parse();
}

void FenParser::parse() {
    std::vector<std::string> sections = Defs::split(fen, ' ');

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

// getters
std::string FenParser::getPiecePlacement() const { return piece_placement_; }
Color FenParser::getActiveColor() const { return active_color_; }
CastleRights FenParser::getCastlingRights() const { return castling_rights_; }
Square FenParser::getEnPassantTarget() const { return en_passant_target_; }
U8 FenParser::getHalfmoveClock() const { return halfmove_clock_; }
U32 FenParser::getFullmoveNumber() const { return fullmove_number_; }

void FenParser::parsePiecePlacement(const std::string& section) {
}

void FenParser::parseActiveColor(const std::string& section) {
    active_color_ = (section == "w") ? WHITE : BLACK;
}

void FenParser::parseCastlingRights(const std::string& section) {
    if (section.find('-') == std::string::npos) {
        if (section.find('K') == std::string::npos) {
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
        en_passant_target_ = Defs::sqFromString (section);
    }
}

void FenParser::parseHalfmove(const std::string& section) {
    halfmove_clock_ = std::stoi(section);
}

void FenParser::parseFullmove(const std::string& section, Color turn) {
    fullmove_number_ = 2 * (std::stoul(section) - 1) + (turn == WHITE ? 0 : 1);
}
