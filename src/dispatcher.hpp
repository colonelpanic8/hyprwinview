#ifndef HYPRWINVIEW_DISPATCHER_HPP
#define HYPRWINVIEW_DISPATCHER_HPP

#define WLR_USE_UNSTABLE

#include <hyprland/src/SharedDefs.hpp>

#include <string>

SDispatchResult onWinviewDispatcher(const std::string& arg);

#endif
