#ifndef HYPRWINVIEW_DISPATCHER_HPP
#define HYPRWINVIEW_DISPATCHER_HPP

#define WLR_USE_UNSTABLE

#include <hyprland/src/SharedDefs.hpp>

#include <string>

#include "overview/command.hpp"

SDispatchResult onWinviewDispatcher(const std::string& arg);
SDispatchResult runWinviewCommand(const Winview::SOverviewCommand& command);

#endif
