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
};

struct UCIOption {
    std::string name;
    std::string key;
    OptionType type;
    OptionValue value;
    std::optional<int> min;
    std::optional<int> max;

    static UCIOption check(std::string name, bool defaultValue);
    static UCIOption spin(std::string name, int defaultValue, int minValue, int maxValue);
    static UCIOption string(std::string name, std::string defaultValue);

    std::string uciDeclaration() const;
    bool setValue(std::string_view rawValue);

    template <typename T>
    const T& getValue() const noexcept {
        return std::get<T>(value);
    }
};
