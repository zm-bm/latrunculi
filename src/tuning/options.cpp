#include "tuning/options.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <format>
#include <ostream>
#include <stdexcept>
#include <type_traits>

namespace tuning {
namespace {
using Parameters = search::Parameters;

template <typename Value>
struct Definition {
    std::string_view name;
    Value initial;
    Value minimum;
    Value maximum;
    Value& (*access)(Parameters&);
};

template <typename Value>
constexpr Definition<Value>
define(std::string_view name, Value low, Value high, Value& (*access)(Parameters&)) {
    auto defaults = search::default_parameters;
    return {name, access(defaults), low, high, access};
}

// Engine limits protect arithmetic and fixed SEE attacker order. Campaigns may narrow them.
constexpr Definition<int> integers[] = {
    define<int>("FutilityMargin1",
                0,
                30000,
                [](Parameters& p) -> int& { return p.futility_margin[1]; }),
    define<int>("FutilityMargin2",
                0,
                30000,
                [](Parameters& p) -> int& { return p.futility_margin[2]; }),
    define<int>("FutilityMargin3",
                0,
                30000,
                [](Parameters& p) -> int& { return p.futility_margin[3]; }),
    define<int>("RazorMargin1", 0, 30000, [](Parameters& p) -> int& { return p.razor_margin[1]; }),
    define<int>("RazorMargin2", 0, 30000, [](Parameters& p) -> int& { return p.razor_margin[2]; }),
    define<int>("RazorMargin3", 0, 30000, [](Parameters& p) -> int& { return p.razor_margin[3]; }),
    define<int>("LateMovePruningMoveCount",
                2,
                64,
                [](Parameters& p) -> int& { return p.late_move_pruning_move_count; }),
    define<int>("NullMoveDepthDivisor",
                1,
                64,
                [](Parameters& p) -> int& { return p.null_move_depth_divisor; }),
    define<int>("NullMoveSurplusPerPly",
                1,
                30000,
                [](Parameters& p) -> int& { return p.null_move_surplus_per_ply; }),
    define<int>("SingularMarginDivisor",
                1,
                1000,
                [](Parameters& p) -> int& { return p.singular_margin_divisor; }),
    define<int>("CaptureLmrHistoryDivisor",
                1024,
                32768,
                [](Parameters& p) -> int& { return p.capture_lmr_history_divisor; }),
    define<int>("QCaptureMargin",
                0,
                30000,
                [](Parameters& p) -> int& { return p.qcapture_margin; }),
    define<int>("CaptureVictimWeight",
                1,
                32,
                [](Parameters& p) -> int& { return p.capture_victim_weight; }),
    define<int>("CaptureHistoryDivisor",
                1,
                256,
                [](Parameters& p) -> int& { return p.capture_history_divisor; }),
    define<int>("CaptureLearningRate",
                1,
                128,
                [](Parameters& p) -> int& { return p.capture_learning_rate; }),
    define<int>("CaptureDepthSquaredLimit",
                1,
                512,
                [](Parameters& p) -> int& { return p.capture_depth_squared_limit; }),
    define<int>("AspirationInitialWindow",
                2,
                10000,
                [](Parameters& p) -> int& { return p.aspiration_initial_window; }),
    define<int>("SeeKnight", 250, 375, [](Parameters& p) -> int& { return p.see_values[KNIGHT]; }),
    define<int>("SeeBishop", 375, 500, [](Parameters& p) -> int& { return p.see_values[BISHOP]; }),
    define<int>("SeeRook", 500, 900, [](Parameters& p) -> int& { return p.see_values[ROOK]; }),
    define<int>("SeeQueen", 900, 1600, [](Parameters& p) -> int& { return p.see_values[QUEEN]; }),
};

constexpr Definition<double> reals[] = {
    define<double>("LmrQuietOffset",
                   0.0,
                   4.0,
                   [](Parameters& p) -> double& { return p.lmr_quiet_offset; }),
    define<double>("LmrCaptureOffset",
                   0.0,
                   4.0,
                   [](Parameters& p) -> double& { return p.lmr_capture_offset; }),
    define<double>("LmrQuietDivisor",
                   0.5,
                   10.0,
                   [](Parameters& p) -> double& { return p.lmr_quiet_divisor; }),
    define<double>("LmrCaptureDivisor",
                   0.5,
                   10.0,
                   [](Parameters& p) -> double& { return p.lmr_capture_divisor; }),
    define<double>("LmrPvScale", 0.1, 1.5, [](Parameters& p) -> double& { return p.lmr_pv_scale; }),
    define<double>("LmrKillerScale",
                   0.1,
                   1.5,
                   [](Parameters& p) -> double& { return p.lmr_killer_scale; }),
    define<double>("CaptureLmrMaxAdjustment",
                   0.0,
                   2.0,
                   [](Parameters& p) -> double& { return p.capture_lmr_max_adjustment; }),
};

bool same_name(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](unsigned char x, unsigned char y) {
        return std::tolower(x) == std::tolower(y);
    });
}

template <typename Value>
bool in_bounds(const Definition<Value>& definition, Value value) noexcept {
    return std::isfinite(value) && value >= definition.minimum && value <= definition.maximum;
}

template <typename Value, std::size_t Size>
bool set_from(Parameters& parameters,
              std::string_view name,
              std::string_view text,
              const Definition<Value> (&definitions)[Size]) {
    for (const auto& definition : definitions) {
        if (!same_name(name, definition.name))
            continue;
        Value value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size())
            throw std::invalid_argument("Invalid numeric tuning value");
        if (!in_bounds(definition, value))
            throw std::out_of_range("Tuning value out of range");
        definition.access(parameters) = value;
        return true;
    }
    return false;
}

template <typename Value, std::size_t Size>
bool valid_values(Parameters& parameters, const Definition<Value> (&definitions)[Size]) noexcept {
    for (const auto& definition : definitions)
        if (!in_bounds(definition, definition.access(parameters)))
            return false;
    return true;
}

template <typename Value, std::size_t Size>
void append_options(std::string& output, const Definition<Value> (&definitions)[Size]) {
    for (const auto& p : definitions) {
        if constexpr (std::is_integral_v<Value>)
            output += std::format(
                "option name {} type spin default {} min {} max {}\n",
                p.name,
                p.initial,
                p.minimum,
                p.maximum);
        else
            // UCI spin options only support integers; fractional values use string options.
            output += std::format("option name {} type string default {}\n", p.name, p.initial);
    }
}

template <typename Value, std::size_t Size>
void export_values(std::ostream& output,
                   bool& first,
                   const Definition<Value> (&definitions)[Size]) {
    for (const auto& p : definitions) {
        if (!first)
            output << ',';
        first = false;
        output << std::format(
            "{{\"name\":\"{}\",\"type\":\"{}\",\"default\":{},\"min\":{},\"max\":{}}}",
            p.name,
            std::is_integral_v<Value> ? "int" : "float",
            p.initial,
            p.minimum,
            p.maximum);
    }
}
} // namespace

bool set(Parameters& parameters, std::string_view name, std::string_view value) {
    return set_from(parameters, name, value, integers) || set_from(parameters, name, value, reals);
}

bool valid(const Parameters& parameters) noexcept {
    auto values = parameters;
    return valid_values(values, integers)
        && valid_values(values, reals)
        && values.see_values[NO_PIECETYPE] == 0
        && values.see_values[PAWN] == 100
        && values.see_values[KING] == 0;
}

std::string uci_options() {
    std::string result;
    append_options(result, integers);
    append_options(result, reals);
    return result;
}

void export_parameters(std::ostream& output) {
    output << "{\"format\":\"latrunculi.search-parameters.v1\",\"parameters\":[";
    bool first = true;
    export_values(output, first, integers);
    export_values(output, first, reals);
    output << "]}\n";
}

} // namespace tuning
