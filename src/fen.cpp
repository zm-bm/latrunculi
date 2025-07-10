#include "fen.hpp"

FenParser::FenParser(const std::string& fen) {
    std::istringstream fenStream(fen);
    std::string piecePlacement, activeColor, castlingRights, enPassantTarget;
    std::string halfmove, fullmove;

    if (!(fenStream >> piecePlacement >> activeColor >> castlingRights >> enPassantTarget)) {
        throw std::invalid_argument("Invalid FEN: must have at least 4 sections");
    }

    parsePieces(piecePlacement);
    parseTurn(activeColor);
    parseCastlingRights(castlingRights);
    parseEnPassantSq(enPassantTarget);

    if (fenStream >> halfmove) {
        parseHalfmove(halfmove);
    }

    if (fenStream >> fullmove) {
        parseFullmove(fullmove, turn);
    }
}

void FenParser::parsePieces(const std::string& section) {
    auto file = File::F1;
    auto rank = Rank::R8;

    for (auto iter = section.begin(); iter != section.end(); ++iter) {
        auto ch = *iter;

        if ((ch >= '1') && (ch <= '8')) {
            file = file + int(ch - '0');
        } else {
            Square sq = makeSquare(file, rank);

            switch (ch) {
                case 'P':
                    pieces.emplace_back(PieceSquare{WHITE, PieceType::Pawn, sq});
                    ++file;
                    break;
                case 'N':
                    pieces.emplace_back(PieceSquare{WHITE, PieceType::Knight, sq});
                    ++file;
                    break;
                case 'B':
                    pieces.emplace_back(PieceSquare{WHITE, PieceType::Bishop, sq});
                    ++file;
                    break;
                case 'R':
                    pieces.emplace_back(PieceSquare{WHITE, PieceType::Rook, sq});
                    ++file;
                    break;
                case 'Q':
                    pieces.emplace_back(PieceSquare{WHITE, PieceType::Queen, sq});
                    ++file;
                    break;
                case 'K':
                    pieces.emplace_back(PieceSquare{WHITE, PieceType::King, sq});
                    ++file;
                    break;
                case 'p':
                    pieces.emplace_back(PieceSquare{BLACK, PieceType::Pawn, sq});
                    ++file;
                    break;
                case 'n':
                    pieces.emplace_back(PieceSquare{BLACK, PieceType::Knight, sq});
                    ++file;
                    break;
                case 'b':
                    pieces.emplace_back(PieceSquare{BLACK, PieceType::Bishop, sq});
                    ++file;
                    break;
                case 'r':
                    pieces.emplace_back(PieceSquare{BLACK, PieceType::Rook, sq});
                    ++file;
                    break;
                case 'q':
                    pieces.emplace_back(PieceSquare{BLACK, PieceType::Queen, sq});
                    ++file;
                    break;
                case 'k':
                    pieces.emplace_back(PieceSquare{BLACK, PieceType::King, sq});
                    ++file;
                    break;
                case '/':
                    --rank;
                    file = File::F1;
                    break;
            }
        }
    }
}

void FenParser::parseTurn(const std::string& section) { turn = (section == "w") ? WHITE : BLACK; }

void FenParser::parseCastlingRights(const std::string& section) {
    if (section.find('-') == std::string::npos) {
        if (section.find('K') != std::string::npos) {
            castle |= WHITE_OO;
        }

        if (section.find('Q') != std::string::npos) {
            castle |= WHITE_OOO;
        }

        if (section.find('k') != std::string::npos) {
            castle |= BLACK_OO;
        }

        if (section.find('q') != std::string::npos) {
            castle |= BLACK_OOO;
        }
    }
}

void FenParser::parseEnPassantSq(const std::string& section) {
    if (section.compare("-") != 0) {
        enPassantSq = makeSquare(section);
    }
}

void FenParser::parseHalfmove(const std::string& section) { hmClock = std::stoi(section); }

void FenParser::parseFullmove(const std::string& section, Color turn) {
    moveCounter = 2 * (std::stoul(section) - 1) + (turn == WHITE ? 0 : 1);
}
