#include "eval/features.hpp"

#include <algorithm>
#include <cassert>
#include <format>
#include <istream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "board/board.hpp"
#include "core/notation.hpp"
#include "eval/parameters.hpp"

namespace eval {

namespace {

TaperedScore weighted_score(const std::array<int, feature::count>& coefficients) noexcept {
    TaperedScore score;
    const auto&  schema = feature_schema();

    for (std::size_t id = 0; id < coefficients.size(); ++id)
        score += schema[id].weight * coefficients[id];

    return score;
}

std::string format_score(TaperedScore score) {
    return std::format("{:5.2f} {:5.2f}",
                       double(score.mg) / int(eval::pawn.mg),
                       double(score.eg) / int(eval::pawn.mg));
}

std::string format_total(TaperedScore score) {
    return std::format(" |  ----  ---- |  ----  ---- | {} | ", format_score(score));
}

std::string format_term(Term term, const TermScore& score) {
    if (term != Term::Material && term != Term::PieceSquare) {
        return std::format(" | {} | {} | {} | ",
                           format_score(score.white),
                           format_score(score.black),
                           format_score(score.total()));
    }
    return format_total(score.total());
}

void write_json_string(std::ostream& output, std::string_view text) {
    constexpr std::string_view hex_digits = "0123456789abcdef";

    output << '"';

    for (const unsigned char ch : text) {
        switch (ch) {
        case '"':  output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (ch < 0x20) {
                output << "\\u00" << hex_digits[ch >> 4] << hex_digits[ch & 0x0f];
            } else {
                output << ch;
            }
        }
    }

    output << '"';
}

std::optional<int> parse_result(std::string_view result) {
    if (result == "1" || result == "1-0")
        return 1;
    if (result == "0" || result == "1/2-1/2")
        return 0;
    if (result == "-1" || result == "0-1")
        return -1;
    return std::nullopt;
}

Board parse_board(std::string_view fen, int line_number) {
    try {
        return Board(fen);
    } catch (const std::invalid_argument& error) {
        throw std::runtime_error(
            std::format("invalid FEN at line {}: {}", line_number, error.what()));
    }
}

void write_schema(std::ostream& output) {
    output << R"({"type":"schema","version":)" << feature_schema_version
           << R"(,"perspective":{"coefficients":"white","fixed":"white","eval":"side_to_move"})"
           << R"(,"result":"1=white_win,0=draw,-1=black_win")"
           << R"(,"phase_counts":["knight","bishop","rook","queen"])"
           << R"(,"pawn_counts":["white","black"],"phase_limit":)" << eval::phase_limit
           << R"(,"phase_material_min":)" << eval::material_eg << R"(,"phase_material_max":)"
           << eval::material_mg << R"(,"scale_limit":)" << eval::scale_limit << R"(,"scale_base":)"
           << eval::scale_base << R"(,"scale_per_pawn":)" << eval::scale_per_pawn << R"(,"tempo":)"
           << eval::tempo_bonus << R"(,"features":[)";

    const auto& schema = feature_schema();
    for (std::size_t id = 0; id < schema.size(); ++id) {
        if (id)
            output << ',';
        output << R"({"id":)" << id << R"(,"name":)";
        write_json_string(output, schema[id].name);
        output << R"(,"mg":)" << schema[id].weight.mg << R"(,"eg":)" << schema[id].weight.eg << '}';
    }

    output << "]}\n";
}

void write_record(std::ostream&        output,
                  std::string_view     source,
                  int                  result,
                  const Board&         board,
                  const FeatureRecord& record) {
    output << R"({"type":"position","version":)" << feature_schema_version << R"(,"source":)";
    write_json_string(output, source);
    output << R"(,"result":)" << result << R"(,"fen":)";
    write_json_string(output, board.to_fen());
    output << R"(,"turn":")" << (record.turn == WHITE ? 'w' : 'b') << R"(","phase_counts":[)";

    for (std::size_t i = 0; i < record.phase_counts.size(); ++i) {
        if (i)
            output << ',';
        output << record.phase_counts[i];
    }

    output << R"(],"pawn_counts":[)" << record.pawn_counts[WHITE] << ','
           << record.pawn_counts[BLACK] << R"(],"fixed":[)" << record.fixed_score.mg << ','
           << record.fixed_score.eg << R"(],"coefficients":[)";

    bool first = true;
    for (std::size_t id = 0; id < record.coefficients.size(); ++id) {
        const int coefficient = record.coefficients[id];
        if (!coefficient)
            continue;
        if (!first)
            output << ',';
        first = false;
        output << '[' << id << ',' << coefficient << ']';
    }

    output << R"(],"eval":)" << record.value << "}\n";
}

} // namespace

TaperedScore TermScore::total() const noexcept {
    return white - black;
}

const TermScore& FeatureRecord::term(Term term) const noexcept {
    return terms[std::to_underlying(term)];
}

TaperedScore FeatureRecord::term_total() const noexcept {
    TaperedScore total;
    for (const TermScore& score : terms)
        total += score.total();
    return total;
}

EvalValue FeatureRecord::white_value() const noexcept {
    return turn == WHITE ? value : -value;
}

EvalValue FeatureRecord::reconstruct() const noexcept {
    TaperedScore score = weighted_score(coefficients) + fixed_score;

    const Color stronger_side = score.eg < 0 ? BLACK : WHITE;
    const int   scale         = std::min(
        eval::scale_limit, eval::scale_base + eval::scale_per_pawn * pawn_counts[stronger_side]);
    score.eg = (score.eg * scale) / eval::scale_limit;

    const auto& schema            = feature_schema();
    const int   non_pawn_material = phase_counts[0] * schema[feature::material(KNIGHT)].weight.mg
                                + phase_counts[1] * schema[feature::material(BISHOP)].weight.mg
                                + phase_counts[2] * schema[feature::material(ROOK)].weight.mg
                                + phase_counts[3] * schema[feature::material(QUEEN)].weight.mg;
    const int material = std::clamp(non_pawn_material, eval::material_eg, eval::material_mg);
    const int phase    = ((material - eval::material_eg) * eval::phase_limit)
                    / (eval::material_mg - eval::material_eg);

    const EvalValue white_value =
        (score.mg * phase + score.eg * (eval::phase_limit - phase)) / eval::phase_limit;
    const EvalValue side_value = turn == WHITE ? white_value : -white_value;
    return side_value + eval::tempo_bonus;
}

void FeatureRecord::complete(const Board& board,
                             TaperedScore unscaled,
                             TaperedScore scaled,
                             EvalValue    tapered_value,
                             EvalValue    side_to_move_value,
                             EvalValue    value) noexcept {
    phase_counts = {
        board.count(WHITE, KNIGHT) + board.count(BLACK, KNIGHT),
        board.count(WHITE, BISHOP) + board.count(BLACK, BISHOP),
        board.count(WHITE, ROOK) + board.count(BLACK, ROOK),
        board.count(WHITE, QUEEN) + board.count(BLACK, QUEEN),
    };
    pawn_counts[WHITE]       = board.count(WHITE, PAWN);
    pawn_counts[BLACK]       = board.count(BLACK, PAWN);
    fixed_score              = unscaled - weighted_score(coefficients);
    unscaled_score           = unscaled;
    scaled_score             = scaled;
    this->tapered_value      = tapered_value;
    this->side_to_move_value = side_to_move_value;
    this->value              = value;
    turn                     = board.side_to_move();
}

const std::array<FeatureDefinition, feature::count>& feature_schema() {
    static const auto schema = [] {
        std::array<FeatureDefinition, feature::count> definitions;

        const auto set = [&](feature::Id id, std::string name, TaperedScore weight) {
            assert(id < definitions.size());
            assert(definitions[id].name.empty());
            definitions[id] = {.name = std::move(name), .weight = weight};
        };

        constexpr std::array<std::string_view, piece_slots> piece_names = {
            "pawn", "knight", "bishop", "rook", "queen", "king"};

        set(feature::material(PAWN), "material.pawn", eval::pawn);
        set(feature::material(KNIGHT), "material.knight", eval::knight);
        set(feature::material(BISHOP), "material.bishop", eval::bishop);
        set(feature::material(ROOK), "material.rook", eval::rook);
        set(feature::material(QUEEN), "material.queen", eval::queen);

        for (PieceType piece = PAWN; piece <= KING; piece = PieceType(piece + 1)) {
            for (Square square = A1; square < INVALID; square = Square(square + 1)) {
                const TaperedScore weight = {
                    eval::piece_squares[piece_slot(piece)][std::to_underlying(Phase::Midgame)]
                                       [square],
                    eval::piece_squares[piece_slot(piece)][std::to_underlying(Phase::Endgame)]
                                       [square],
                };
                set(feature::piece_square(piece, square),
                    "psqt." + std::string(piece_names[piece_slot(piece)]) + "." + to_string(square),
                    weight);
            }
        }

        set(feature::isolated_pawn, "pawn.isolated", eval::isolated_pawn);
        set(feature::backward_pawn, "pawn.backward", eval::backward_pawn);
        set(feature::doubled_pawn, "pawn.doubled", eval::doubled_pawn);
        for (Rank rank = RANK1; rank <= RANK8; rank = Rank(rank + 1))
            set(feature::passed_pawn(rank),
                "pawn.passed.r" + std::string(1, to_char(rank)),
                eval::passed_pawn[rank]);

        set(feature::piece_feature(feature::PieceFeature::ReachableOutpost),
            "piece.reachable_outpost",
            eval::reachable_outpost);
        set(feature::piece_feature(feature::PieceFeature::BishopOutpost),
            "piece.bishop_outpost",
            eval::bishop_outpost);
        set(feature::piece_feature(feature::PieceFeature::KnightOutpost),
            "piece.knight_outpost",
            eval::knight_outpost);
        set(feature::piece_feature(feature::PieceFeature::MinorPawnShield),
            "piece.minor_pawn_shield",
            eval::minor_pawn_shield);
        set(feature::piece_feature(feature::PieceFeature::BishopLongDiagonal),
            "piece.bishop_long_diagonal",
            eval::bishop_long_diagonal);
        set(feature::piece_feature(feature::PieceFeature::BishopPair),
            "piece.bishop_pair",
            eval::bishop_pair);
        set(feature::piece_feature(feature::PieceFeature::BishopBlockers),
            "piece.bishop_blockers",
            eval::bishop_blockers);
        set(feature::piece_feature(feature::PieceFeature::RookClosedFile),
            "piece.rook_closed_file",
            eval::rook_closed_file);
        set(feature::piece_feature(feature::PieceFeature::KingZoneXrayAttack),
            "piece.king_zone_xray_attack",
            eval::king_zone_xray_attack);
        set(feature::piece_feature(feature::PieceFeature::QueenDiscoveredAttack),
            "piece.queen_discovered_attack",
            eval::queen_discovered_attack);

        set(feature::rook_open(false), "piece.rook_semi_open_file", eval::rook_open_file[0]);
        set(feature::rook_open(true), "piece.rook_open_file", eval::rook_open_file[1]);

        const auto add_mobility = [&](PieceType piece, std::string_view name, const auto& weights) {
            for (std::size_t moves = 0; moves < std::size(weights); ++moves)
                set(feature::mobility(piece, int(moves)),
                    "mobility." + std::string(name) + "." + std::to_string(moves),
                    weights[moves]);
        };
        add_mobility(KNIGHT, "knight", eval::knight_mob);
        add_mobility(BISHOP, "bishop", eval::bishop_mob);
        add_mobility(ROOK, "rook", eval::rook_mob);
        add_mobility(QUEEN, "queen", eval::queen_mob);

        set(feature::weak_piece(KNIGHT), "threat.weak_knight", eval::weak_piece[KNIGHT]);
        set(feature::weak_piece(BISHOP), "threat.weak_bishop", eval::weak_piece[BISHOP]);
        set(feature::weak_piece(ROOK), "threat.weak_rook", eval::weak_piece[ROOK]);
        set(feature::weak_piece(QUEEN), "threat.weak_queen", eval::weak_piece[QUEEN]);

        assert(std::ranges::all_of(definitions, [](const FeatureDefinition& definition) {
            return !definition.name.empty();
        }));
        return definitions;
    }();

    return schema;
}

std::string format_evaluation(const FeatureRecord& record) {
    std::string output = "     Term    |    White    |    Black    |    Total   \n"
                         "             |   MG    EG  |   MG    EG  |   MG    EG \n"
                         " ------------+-------------+-------------+------------\n";

    const auto append_term = [&](std::string_view name, Term term) {
        output += std::format("{:>12}{}\n", name, format_term(term, record.term(term)));
    };

    append_term("Material", Term::Material);
    append_term("Piece Sq.", Term::PieceSquare);
    append_term("Pawns", Term::Pawns);
    append_term("Knights", Term::Knights);
    append_term("Bishops", Term::Bishops);
    append_term("Rooks", Term::Rooks);
    append_term("Queens", Term::Queens);
    append_term("King Safety", Term::KingSafety);
    append_term("Mobility", Term::Mobility);
    append_term("Threats", Term::Threats);

    output += " ------------+-------------+-------------+------------\n";
    output += std::format("{:>12}{}\n\n", "Total", format_total(record.scaled_score));
    output +=
        std::format("Evaluation:\t{:.2f}\n", double(record.white_value()) / int(eval::pawn.mg));
    return output;
}

void export_features(std::istream& input, std::ostream& output, const PositionPreparer& prepare) {
    write_schema(output);

    std::string line;
    int         line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty())
            continue;

        const std::size_t first_tab = line.find('\t');
        const std::size_t second_tab =
            first_tab == std::string::npos ? std::string::npos : line.find('\t', first_tab + 1);
        if (first_tab == std::string::npos || second_tab == std::string::npos || first_tab == 0)
            throw std::runtime_error(std::format("invalid input at line {}", line_number));

        const std::string_view source{line.data(), first_tab};
        const std::string_view result_text{line.data() + first_tab + 1, second_tab - first_tab - 1};
        const std::string_view fen{line.data() + second_tab + 1, line.size() - second_tab - 1};
        const auto             result = parse_result(result_text);
        if (!result || fen.empty())
            throw std::runtime_error(std::format("invalid input at line {}", line_number));

        Board board = parse_board(fen, line_number);
        if (prepare && !prepare(board))
            continue;

        const FeatureRecord record = extract_features(board);
        write_record(output, source, *result, board, record);
    }
}

} // namespace eval
