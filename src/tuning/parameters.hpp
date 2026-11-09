#pragma once

#include <array>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>

namespace tuning {

// One definition supplies compile-time defaults, runtime storage, and export metadata.
// Runtime settings belong to the engine. Its idle guard protects all search readers.
struct Parameters {
#if LATRUNCULI_TUNING
#define PARAMETER(type, name, value, minimum, maximum, search) type name = value;
#else
#define PARAMETER(type, name, value, minimum, maximum, search) static constexpr type name = value;
#endif
#include "tuning/parameters.inc"
#undef PARAMETER

    constexpr int futility_margin(int depth) const {
        return std::array{0, FutilityMargin1, FutilityMargin2, FutilityMargin3}[depth];
    }
    constexpr int razor_margin(int depth) const {
        return std::array{0, RazorMargin1, RazorMargin2, RazorMargin3}[depth];
    }
};

inline constexpr Parameters defaults{};

struct Definition {
    std::string_view name;
    bool integer;
    double initial;
    double minimum;
    double maximum;
    bool search;
};

std::span<const Definition> definitions();
// Returns false for an unknown option; throws for invalid values without mutation.
bool set(Parameters& parameters, std::string_view name, std::string_view value);
std::string uci_options();
void export_spsa(std::ostream& output, std::string_view group);

} // namespace tuning
