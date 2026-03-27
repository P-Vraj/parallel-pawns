#pragma once
#include <format>
#include <optional>
#include <string>
#include <variant>

using OptionValue = std::variant<bool, int, std::string>;

constexpr std::string to_string(const OptionValue& option) {
    if (std::holds_alternative<bool>(option))
        return std::format("{}", std::get<bool>(option));
    if (std::holds_alternative<int>(option))
        return std::to_string(std::get<int>(option));
    if (std::holds_alternative<std::string>(option))
        return std::get<std::string>(option);
    return std::string{};
}

enum class OptionType : uint8_t {
    Check,   // bool
    Spin,    // int
    String,  // string

    Count = 3
};

struct UCIOption {
    static UCIOption check(std::string_view name, bool defaultValue);
    static UCIOption spin(std::string_view name, int defaultValue, int minValue, int maxValue);
    static UCIOption string(std::string_view name, std::string defaultValue);

    std::string uciDeclaration() const;
    bool setValue(std::string_view rawValue);
    template <typename T>
    const T& getValue() const {
        return std::get<T>(value_);
    }

    std::string name() const { return name_; }
    std::string key() const { return key_; }

private:
    std::string name_;
    std::string key_;
    OptionType type_{OptionType::Check};
    OptionValue value_;
    std::optional<int> min_{std::nullopt};
    std::optional<int> max_{std::nullopt};
};
