#ifndef HYPRWINVIEW_WORKSPACE_MODULE_HPP
#define HYPRWINVIEW_WORKSPACE_MODULE_HPP

#include <hyprland/src/plugins/PluginAPI.hpp>

void initializeWorkspaceOverview(HANDLE handle);
void shutdownWorkspaceOverview();

bool workspaceOverviewActive();
void closeWorkspaceOverview();
void closeWorkspaceOverviewImmediately();

#endif
