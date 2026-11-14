#include "tuning/options.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <ostream>
#include <stdexcept>

namespace tuning {
namespace {
using Parameters = search::Parameters;
constexpr auto defaults = search::default_parameters;

struct Definition {
    std::string_view name;
    int initial;
    int minimum;
    int maximum;
    int& (*access)(Parameters&);
};

// Limits protect engine arithmetic. A tuning recipe may choose narrower bounds.
constexpr Definition definitions[] = {
    {"FutilityMargin1",
     defaults.futility_margin[1],
     0,
     30000,
     [](Parameters& p) -> int& { return p.futility_margin[1]; }},
    {"FutilityMargin2",
     defaults.futility_margin[2],
     0,
     30000,
     [](Parameters& p) -> int& { return p.futility_margin[2]; }},
    {"FutilityMargin3",
     defaults.futility_margin[3],
     0,
     30000,
     [](Parameters& p) -> int& { return p.futility_margin[3]; }},
    {"RazorMargin1",
     defaults.razor_margin[1],
     0,
     30000,
     [](Parameters& p) -> int& { return p.razor_margin[1]; }},
    {"RazorMargin2",
     defaults.razor_margin[2],
     0,
     30000,
     [](Parameters& p) -> int& { return p.razor_margin[2]; }},
    {"RazorMargin3",
     defaults.razor_margin[3],
     0,
     30000,
     [](Parameters& p) -> int& { return p.razor_margin[3]; }},
    {"NullMoveSurplusPerPly",
     defaults.null_move_surplus_per_ply,
     1,
     30000,
     [](Parameters& p) -> int& { return p.null_move_surplus_per_ply; }},
    {"QCaptureMargin",
     defaults.qcapture_margin,
     0,
     30000,
     [](Parameters& p) -> int& { return p.qcapture_margin; }},
};
} // namespace

bool set(Parameters& parameters, std::string_view name, std::string_view text) {
    for (const auto& definition : definitions) {
        const bool match =
            std::ranges::equal(name, definition.name, [](unsigned char a, unsigned char b) {
                return std::tolower(a) == std::tolower(b);
            });
        if (!match)
            continue;
        int value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size())
            throw std::invalid_argument("Invalid integer tuning value");
        if (value < definition.minimum || value > definition.maximum)
            throw std::out_of_range("Tuning value out of range");
        definition.access(parameters) = value;
        return true;
    }
    return false;
}

std::string uci_options() {
    std::string result;
    for (const auto& p : definitions)
        result += std::format(
            "option name {} type spin default {} min {} max {}\n",
            p.name,
            p.initial,
            p.minimum,
            p.maximum);
    return result;
}

void export_parameters(std::ostream& output) {
    output << "{\"format\":\"latrunculi.search-parameters.v1\",\"parameters\":[";
    bool first = true;
    for (const auto& p : definitions) {
        if (!first)
            output << ',';
        first = false;
        output << std::format(
            "{{\"name\":\"{}\",\"type\":\"int\",\"default\":{},\"min\":{},\"max\":{}}}",
            p.name,
            p.initial,
            p.minimum,
            p.maximum);
    }
    output << "]}\n";
}

} // namespace tuning
