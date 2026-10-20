#ifndef HYPRWINVIEW_OVERVIEW_FILTER_HPP
#define HYPRWINVIEW_OVERVIEW_FILTER_HPP

#include "../strings.hpp"
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace Winview {
    inline std::vector<std::string> queryTokens(const std::string& query) {
        std::vector<std::string> result;
        std::stringstream        stream(lowercase(query));
        std::string              token;
        while (stream >> token)
            result.push_back(token);
        return result;
    }

    inline bool matchesQuery(const std::string&              searchable,
                             const std::vector<std::string>& tokens) {
        return std::ranges::all_of(tokens, [&searchable](const std::string& token) {
            return searchable.find(token) != std::string::npos;
        });
    }
}

#endif
