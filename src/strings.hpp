#ifndef HYPRWINVIEW_STRINGS_HPP
#define HYPRWINVIEW_STRINGS_HPP

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace Winview {
    inline std::string trim(std::string_view text) {
        auto       notSpace = [](unsigned char c) { return !std::isspace(c); };
        const auto BEGIN    = std::ranges::find_if(text, notSpace);
        const auto END      = std::ranges::find_if(text.rbegin(), text.rend(), notSpace).base();
        return std::string(BEGIN, BEGIN < END ? END : BEGIN);
    }

    inline std::string lowercase(std::string text) {
        std::ranges::transform(text, text.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    inline std::string trimmedLower(std::string_view text) {
        return lowercase(trim(text));
    }
}

#endif
