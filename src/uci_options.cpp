#include "uci_options.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace uci {

namespace {

std::string lower_ascii(std::string value) {
    std::ranges::transform(
        value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

} // namespace

void SpinOption::set(std::string value_str) {
    size_t pos = 0;
    int    val = std::stoi(value_str, &pos);
    while (pos < value_str.size() && std::isspace(static_cast<unsigned char>(value_str[pos])))
        ++pos;
    if (pos != value_str.size())
        throw std::invalid_argument("Invalid integer value");
    if (val < min_value || val > max_value)
        throw std::out_of_range("Value out of range");
    value = val;
}

void CheckOption::set(std::string val_str) {
    val_str = lower_ascii(std::move(val_str));
    if (val_str == "true" || val_str == "on") {
        value = true;
    } else if (val_str == "false" || val_str == "off") {
        value = false;
    } else {
        throw std::invalid_argument("Invalid boolean value");
    }
}

OptionId Options::set(const std::string& name, const std::string& value, bool has_value) {
    const std::string option_name = lower_ascii(name);

    auto require_value = [&](std::string_view option) {
        if (!has_value)
            throw std::invalid_argument(std::string(option) + " requires a value");
    };

    if (option_name == "hash") {
        require_value("Hash");
        hash.set(value);
        return OptionId::Hash;
    } else if (option_name == "threads") {
        require_value("Threads");
        threads.set(value);
        return OptionId::Threads;
    } else if (option_name == "debug") {
        require_value("Debug");
        debug.set(value);
        return OptionId::Debug;
    } else if (option_name == "clear hash") {
        if (has_value)
            throw std::invalid_argument("Clear Hash does not take a value");
        return OptionId::ClearHash;
    } else {
        throw std::invalid_argument("Unknown UCI option: " + name);
    }
}

} // namespace uci
