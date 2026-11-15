#pragma once

#include <array>

#include "board/see_values.hpp"

namespace search {

// Values used by search, ordering and learning. Recipes own campaign bounds and schedules.
struct Parameters {
    std::array<int, 4> futility_margin{0, 250, 400, 550};
    std::array<int, 4> razor_margin{0, 500, 900, 1800};
    int late_move_pruning_move_count = 12;
    int null_move_depth_divisor = 7;
    int null_move_surplus_per_ply = 200;
    int singular_margin_divisor = 50;

    double lmr_quiet_offset = 1.25;
    double lmr_capture_offset = 0.75;
    double lmr_quiet_divisor = 2.5;
    double lmr_capture_divisor = 3.3;
    double lmr_pv_scale = 0.7;
    double lmr_killer_scale = 0.8;
    int capture_lmr_history_divisor = 8192;
    double capture_lmr_max_adjustment = 1.0;

    int qcapture_margin = 300;
    int capture_victim_weight = 7;
    int capture_history_divisor = 16;
    int capture_learning_rate = 16;
    int capture_depth_squared_limit = 128;
    int aspiration_initial_window = 50;
    see::Values see_values = see::piece_values;
};

inline constexpr Parameters default_parameters{};

} // namespace search
