#include "ucioption.h"

#include <algorithm>
#include <charconv>

#include "util.h"

UCIOption UCIOption::check(std::string_view name, bool defaultValue) {
    UCIOption uciOption;
    uciOption.name_ = name;
    uciOption.key_ = normalized_option_key(name);
    uciOption.type_ = OptionType::Check;
    uciOption.value_ = defaultValue;
    return uciOption;
}

UCIOption UCIOption::spin(std::string_view name, int defaultValue, int minValue, int maxValue) {
    UCIOption uciOption;
    uciOption.name_ = name;
    uciOption.key_ = normalized_option_key(name);
    uciOption.type_ = OptionType::Spin;
    uciOption.value_ = defaultValue;
    uciOption.min_ = minValue;
    uciOption.max_ = maxValue;
    return uciOption;
}

UCIOption UCIOption::string(std::string_view name, std::string defaultValue) {
    UCIOption uciOption;
    uciOption.name_ = name;
    uciOption.key_ = normalized_option_key(name);
    uciOption.type_ = OptionType::String;
    uciOption.value_ = defaultValue;
    uciOption.min_ = std::nullopt;
    uciOption.max_ = std::nullopt;
    return uciOption;
}

std::string UCIOption::uciDeclaration() const {
    std::string out{std::format("option name {}", name_)};
    switch (type_) {
        case OptionType::Check:
            out += std::format(" type check default {}", to_string(value_));
            break;
        case OptionType::Spin:
            if (min_.has_value() && max_.has_value())
                out += std::format(" type spin default {} min {} max {}", to_string(value_), *min_, *max_);
            break;
        case OptionType::String:
            out += std::format(" type string default {}", to_string(value_));
            break;
    }
    return out;
}

bool UCIOption::setValue(std::string_view rawValue) {
    switch (type_) {
        case OptionType::Check: {
            if (rawValue.empty() || rawValue == "true" || rawValue == "on") {
                value_ = true;
                return true;
            }
            if (rawValue == "false" || rawValue == "off") {
                value_ = false;
                return true;
            }

            return false;
        }
        case OptionType::Spin: {
            int parsedValue{};
            const auto* begin = rawValue.data();
            const auto* end = begin + rawValue.size();
            const auto [ptr, ec] = std::from_chars(begin, end, parsedValue);
            if (ec != std::errc{} || ptr != end)
                return false;
            if (!min_.has_value() || parsedValue < *min_ || !max_.has_value() || parsedValue > *max_)
                return false;

            value_ = parsedValue;
            return true;
        }
        case OptionType::String: {
            value_ = std::string(rawValue);
            return true;
        }
    }

    return false;
}
