#pragma once

#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>

#include "base.hpp"
#include "search_stats.hpp"
#include "types.hpp"

// --------------------------
// UCI Protocol Implementation
// https://www.wbec-ridderkerk.nl/html/UCIProtocol.html
// --------------------------

class UCIOptionBase {
   public:
    virtual ~UCIOptionBase()                        = default;
    virtual void setValue(const std::string& value) = 0;
    virtual void print(std::ostream& os) const      = 0;

    friend std::ostream& operator<<(std::ostream& os, const UCIOptionBase& option) {
        option.print(os);
        return os;
    }
};

template <typename T>
class UCIOption : public UCIOptionBase {
   public:
    using Callback = std::function<void(const T&)>;

    T value;
    T defaultValue;
    std::optional<T> minValue;
    std::optional<T> maxValue;
    Callback onChange;

    UCIOption(T defaultValue,
              std::optional<T> min = std::nullopt,
              std::optional<T> max = std::nullopt,
              Callback onChange    = nullptr)
        : value(defaultValue),
          defaultValue(defaultValue),
          minValue(min),
          maxValue(max),
          onChange(onChange) {}

    void updateSpin(const std::string& strValue) {
        auto newValue = std::stoi(strValue);
        if (minValue && newValue < *minValue) throw std::out_of_range("Value below minimum");
        if (maxValue && newValue > *maxValue) throw std::out_of_range("Value above maximum");
        value = newValue;
    }

    void updateCheck(const std::string& strValue) {
        if (strValue == "true" || strValue == "on") {
            value = true;
        } else if (strValue == "false" || strValue == "off") {
            value = false;
        } else {
            throw std::invalid_argument("Invalid boolean value");
        }
    }

    void setValue(const std::string& strValue) override {
        T prevValue = value;

        if constexpr (std::is_same_v<T, int>) {
            updateSpin(strValue);
        } else if constexpr (std::is_same_v<T, bool>) {
            updateCheck(strValue);
        } else if constexpr (std::is_same_v<T, std::string>) {
            value = strValue;
        }

        if (onChange && value != prevValue) {
            onChange(value);
        }
    }

    void print(std::ostream& os) const override {
        auto typeName = std::is_same_v<T, int>    ? "spin"
                        : std::is_same_v<T, bool> ? "check"
                                                  : "string";
        os << "type " << typeName;

        if constexpr (std::is_same_v<T, bool>) {
            os << " default " << (defaultValue ? "true" : "false");
        } else {
            os << " default " << defaultValue;
        }

        if (minValue) os << " min " << *minValue;
        if (maxValue) os << " max " << *maxValue;
    }
};

struct UCIConfig {
    using OptionMap = std::unordered_map<std::string, std::unique_ptr<UCIOptionBase>>;

    OptionMap options;

    template <typename T>
    void registerOption(const std::string& name,
                        T defaultValue,
                        std::optional<T> min                     = std::nullopt,
                        std::optional<T> max                     = std::nullopt,
                        typename UCIOption<T>::Callback callback = nullptr) {
        options[name] = std::make_unique<UCIOption<T>>(defaultValue, min, max, callback);
    }

    template <typename T>
    T getOption(const std::string& name) const {
        auto it = options.find(name);
        if (it == options.end()) {
            throw std::invalid_argument("Unknown UCI option: " + name);
        }

        auto* typedOpt = dynamic_cast<UCIOption<T>*>(it->second.get());
        if (!typedOpt) {
            throw std::runtime_error("Type mismatch for option: " + name);
        }

        return typedOpt->value;
    }

    void setOption(const std::string& name, const std::string& value) {
        auto it = options.find(name);
        if (it == options.end()) {
            throw std::invalid_argument("Unknown UCI option: " + name);
        }
        it->second->setValue(value);
    }

    friend std::ostream& operator<<(std::ostream& os, const UCIConfig& config) {
        for (const auto& [name, option] : config.options) {
            os << "option name " << name << " " << *option << "\n";
        }
        return os;
    }
};

struct UCIBestLine {
    UCIBestLine(int score, int depth, U64 nodes, Milliseconds time, std::string& pv)
        : score(score), depth(depth), nodes(nodes), time(time), pv(pv) {}

    std::string formatScore() const;
    friend std::ostream& operator<<(std::ostream& os, const UCIBestLine& info);

    int score;
    int depth;
    U64 nodes;
    Milliseconds time;
    std::string pv;
};

class UCIProtocolHandler {
   public:
    explicit UCIProtocolHandler(std::ostream& out, std::ostream& err) : out(out), err(err) {}

    void help() const;
    void identify(const UCIConfig& config) const;
    void ready() const;
    void bestmove(std::string) const;
    void info(const UCIBestLine& info) const;
    void info(const std::string& str) const;

    template <typename T>
    void logOutput(T&& obj) const {
        err << std::forward<T>(obj) << std::endl;
    }

   private:
    std::ostream& out;
    std::ostream& err;
};
