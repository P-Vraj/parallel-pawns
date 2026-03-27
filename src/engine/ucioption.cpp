#include "ucioption.h"

#include <algorithm>
#include <charconv>

#include "util.h"

UCIOption UCIOption::check(std::string name, bool defaultValue) {
    return UCIOption{
        .name = name,
        .key = normalized_option_key(name),
        .type = OptionType::Check,
        .value = defaultValue,
        .min = std::nullopt,
        .max = std::nullopt
    };
}

UCIOption UCIOption::spin(std::string name, int defaultValue, int minValue, int maxValue) {
    return UCIOption{
        .name = name,
        .key = normalized_option_key(name),
        .type = OptionType::Spin,
        .value = defaultValue,
        .min = minValue,
        .max = maxValue
    };
}

UCIOption UCIOption::string(std::string name, std::string defaultValue) {
    return UCIOption{
        .name = name,
        .key = normalized_option_key(name),
        .type = OptionType::String,
        .value = std::move(defaultValue),
        .min = std::nullopt,
        .max = std::nullopt
    };
}

std::string UCIOption::uciDeclaration() const {
    std::string out{std::format("option name {}", name)};
    switch (type) {
        case OptionType::Check:
            out += std::format(" type check default {}", to_string(value));
            break;
        case OptionType::Spin:
            out += std::format(" type spin default {} min {} max {}", to_string(value), *min, *max);
            break;
        case OptionType::String:
            out += std::format(" type string default {}", to_string(value));
            break;
    }
    return out;
}

bool UCIOption::setValue(std::string_view rawValue) {
    switch (type) {
        case OptionType::Check: {
            if (rawValue.empty() || rawValue == "true" || rawValue == "on") {
                value = true;
                return true;
            }
            if (rawValue == "false" || rawValue == "off") {
                value = false;
                return true;
            }

            return false;
        }
        case OptionType::Spin: {
            int parsedValue{};
            const auto* begin = rawValue.data();
            const auto* end = begin + rawValue.size();
            const auto [ptr, ec] = std::from_chars(begin, end, parsedValue);
            if (ec != std::errc{} || ptr != end || parsedValue < *min || parsedValue > *max)
                return false;

            value = parsedValue;
            return true;
        }
        case OptionType::String: {
            value = std::string(rawValue);
            return true;
        }
    }

    return false;
}
