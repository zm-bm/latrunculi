#include "tuning/parameters.hpp"

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
constexpr Definition registry[] = {
#define PARAMETER(type, name, value, minimum, maximum, search)                                     \
    {#name, std::is_integral_v<type>, value, minimum, maximum, search},
#include "tuning/parameters.inc"
#undef PARAMETER
};

#if LATRUNCULI_TUNING
bool same_name(std::string_view lhs, std::string_view rhs) {
    return std::ranges::equal(lhs, rhs, [](unsigned char a, unsigned char b) {
        return std::tolower(a) == std::tolower(b);
    });
}

template <typename T>
T parse(std::string_view text, T minimum, T maximum) {
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value))
        throw std::invalid_argument("Invalid tuning value");
    if (value < minimum || value > maximum)
        throw std::out_of_range("Tuning value out of range");
    return value;
}
#endif
} // namespace

std::span<const Definition> definitions() {
    return registry;
}

bool set(Parameters& parameters, std::string_view name, std::string_view value) {
#if LATRUNCULI_TUNING
#define PARAMETER(type, field, initial, minimum, maximum, search)                                  \
    if (same_name(name, #field)) {                                                                 \
        parameters.field = parse<type>(value, minimum, maximum);                                   \
        return true;                                                                               \
    }
#include "tuning/parameters.inc"
#undef PARAMETER
#endif
    return false;
}

std::string uci_options() {
    std::string result;
#if LATRUNCULI_TUNING
    for (const auto& p : registry) {
        result += p.integer
            ? std::format("option name {} type spin default {} min {} max {}\n",
                          p.name,
                          p.initial,
                          p.minimum,
                          p.maximum)
            : std::format("option name {} type string default {}\n", p.name, p.initial);
    }
#endif
    return result;
}

void export_spsa(std::ostream& output, std::string_view group) {
#if !LATRUNCULI_TUNING
    throw std::runtime_error("SPSA export requires LATRUNCULI_TUNING=ON");
#else
    if (group != "search" && group != "all")
        throw std::invalid_argument("SPSA group must be search or all");
    bool first = true;
    for (const auto& p : registry) {
        if (group == "search" && !p.search)
            continue;
        if (!first)
            output << '\n';
        first = false;
        const double c = std::max((p.maximum - p.minimum) / 20.0, p.integer ? 0.5 : 0.0);
        output << std::format(
            "{}, {}, {}, {}, {}, {}, 0.002",
            p.name,
            p.integer ? "int" : "float",
            p.initial,
            p.minimum,
            p.maximum,
            c);
    }
#endif
}

} // namespace tuning
