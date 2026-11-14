#pragma once

#include <array>

namespace search {

// Search values only. Campaign selection, bounds and schedules belong to tools/.
struct Parameters {
    std::array<int, 4> futility_margin{0, 250, 400, 550};
    std::array<int, 4> razor_margin{0, 500, 900, 1800};
    int null_move_surplus_per_ply = 200;
    int qcapture_margin = 300;
};

inline constexpr Parameters default_parameters{};

} // namespace search
