#pragma once

#include <string>

#include "core/defs.hpp"

namespace uci {

// UCI options and engine configuration.
struct SpinOption {
    int  value;
    int  default_value;
    int  min_value;
    int  max_value;
    void set(std::string value_str);
};

struct CheckOption {
    bool value;
    bool default_value;
    void set(std::string val_str);
};

struct ButtonOption {};

enum class OptionId { Hash, Threads, Debug, ClearHash };

struct Options {
    SpinOption hash = {
        .value         = DEFAULT_HASH,
        .default_value = DEFAULT_HASH,
        .min_value     = 1,
        .max_value     = 2048,
    };

    SpinOption threads = {
        .value         = DEFAULT_THREADS,
        .default_value = DEFAULT_THREADS,
        .min_value     = 1,
        .max_value     = 64,
    };

    CheckOption debug = {
        .value         = false,
        .default_value = false,
    };

    ButtonOption clear_hash;

    OptionId set(const std::string& name, const std::string& value, bool has_value);
};

} // namespace uci
