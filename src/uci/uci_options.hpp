#pragma once

#include <string>

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
    static constexpr int default_hash_mb = 4;
    static constexpr int default_threads = 1;

    SpinOption hash = {
        .value         = default_hash_mb,
        .default_value = default_hash_mb,
        .min_value     = 1,
        .max_value     = 2048,
    };

    SpinOption threads = {
        .value         = default_threads,
        .default_value = default_threads,
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
