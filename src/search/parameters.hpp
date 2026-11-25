#pragma once

#include <array>

#include "board/see_values.hpp"

namespace search {

// Values used by search, ordering and learning. Recipes own campaign bounds and schedules.
// Rounded defaults from OpenBench SPSA #107, warm-started from #106.
struct Parameters {
    std::array<int, 4> futility_margin{0, 220, 385, 535};
    std::array<int, 4> razor_margin{0, 515, 870, 1770};
    int late_move_pruning_move_count = 12;
    int null_move_depth_divisor = 5;
    int null_move_surplus_per_ply = 193;
    int singular_margin_divisor = 50;

    double lmr_quiet_offset = 1.2182;
    double lmr_capture_offset = 0.6986;
    double lmr_quiet_divisor = 2.3578;
    double lmr_capture_divisor = 3.3958;
    double lmr_pv_scale = 0.7063;
    double lmr_killer_scale = 0.7862;
    int capture_lmr_history_divisor = 8192;
    double capture_lmr_max_adjustment = 0.9655;

    int qcapture_margin = 355;
    int capture_victim_weight = 7;
    int capture_history_divisor = 16;
    int capture_learning_rate = 16;
    int capture_depth_squared_limit = 127;
    int aspiration_initial_window = 52;
    see::Values see_values = see::piece_values;
};

inline constexpr Parameters default_parameters{};

} // namespace search
