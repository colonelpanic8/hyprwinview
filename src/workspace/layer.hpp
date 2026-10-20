#pragma once

#include <charconv>
#include <cctype>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace HT {
    inline std::expected<int64_t, std::string> targetLayer(std::string_view argument, int current,
                                                           int count, bool wrap) {
        while (!argument.empty() && std::isspace(static_cast<unsigned char>(argument.front())))
            argument.remove_prefix(1);
        while (!argument.empty() && std::isspace(static_cast<unsigned char>(argument.back())))
            argument.remove_suffix(1);
        if (argument.empty())
            return std::unexpected("expected a layer number or signed offset");
        if (count <= 0)
            return std::unexpected("grid:layers must be positive");

        const bool RELATIVE = argument.front() == '+' || argument.front() == '-';
        if (argument.front() == '+') {
            argument.remove_prefix(1);
            if (argument.empty() || argument.front() == '-')
                return std::unexpected("invalid layer number");
        }

        int value = 0;
        const auto [END, ERROR] =
            std::from_chars(argument.data(), argument.data() + argument.size(), value);
        if (ERROR != std::errc{} || END != argument.data() + argument.size())
            return std::unexpected("invalid layer number");

        int64_t target = value;
        if (RELATIVE)
            target += current;
        if (wrap && (target < 0 || target >= count))
            target = (target % count + count) % count;
        return target;
    }
}
