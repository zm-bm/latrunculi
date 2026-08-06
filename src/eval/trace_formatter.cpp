#include "eval/trace_formatter.hpp"

#include <format>
#include <string_view>

#include "eval/parameters.hpp"

namespace eval {
namespace {

std::string format_score(TaperedScore score) {
    return std::format("{:5.2f} {:5.2f}",
                       double(score.mg) / int(eval::pawn.mg),
                       double(score.eg) / int(eval::pawn.mg));
}

std::string format_term(const TermScore& term) {
    if (term.has_both) {
        return std::format(" | {} | {} | {} | ",
                           format_score(term.white),
                           format_score(term.black),
                           format_score(term.total()));
    }
    return std::format(" |  ----  ---- |  ----  ---- | {} | ", format_score(term.white));
}

} // namespace

std::string format_trace(const Trace& trace) {
    std::string output = "     Term    |    White    |    Black    |    Total   \n"
                         "             |   MG    EG  |   MG    EG  |   MG    EG \n"
                         " ------------+-------------+-------------+------------\n";

    const auto append_term = [&](std::string_view name, Term term) {
        output += std::format("{:>12}{}\n", name, format_term(trace.term(term)));
    };

    append_term("Material", Term::Material);
    append_term("Piece Sq.", Term::Squares);
    append_term("Pawns", Term::Pawns);
    append_term("Knights", Term::Knights);
    append_term("Bishops", Term::Bishops);
    append_term("Rooks", Term::Rooks);
    append_term("Queens", Term::Queens);
    append_term("Kings", Term::King);
    append_term("Mobility", Term::Mobility);
    append_term("Threats", Term::Threats);

    output += " ------------+-------------+-------------+------------\n";
    output +=
        std::format("{:>12}{}\n\n", "Total", format_term(TermScore{.white = trace.final_score()}));
    output +=
        std::format("Evaluation:\t{:.2f}\n", double(trace.white_value()) / int(eval::pawn.mg));
    return output;
}

} // namespace eval
