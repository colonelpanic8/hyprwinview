#ifndef HYPRWINVIEW_OVERVIEW_GROUPING_HPP
#define HYPRWINVIEW_OVERVIEW_GROUPING_HPP

#include <string>
#include <unordered_map>
#include <vector>

namespace Winview {
    inline std::vector<size_t> stableGroupOrder(const std::vector<std::string>& keys) {
        std::unordered_map<std::string, size_t> groupIndices;
        std::vector<std::vector<size_t>>        groups;
        for (size_t i = 0; i < keys.size(); ++i) {
            const auto [it, inserted] = groupIndices.try_emplace(keys[i], groups.size());
            if (inserted)
                groups.emplace_back();
            groups[it->second].push_back(i);
        }
        std::vector<size_t> order;
        order.reserve(keys.size());
        for (const auto& group : groups)
            order.insert(order.end(), group.begin(), group.end());
        return order;
    }
}

#endif
