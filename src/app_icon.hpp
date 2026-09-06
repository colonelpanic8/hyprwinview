#ifndef HYPRWINVIEW_APP_ICON_HPP
#define HYPRWINVIEW_APP_ICON_HPP

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Texture.hpp>

SP<Render::ITexture> appIconTextureForWindow(const PHLWINDOW& window, int sizePx);
void                 initializeAppIconCache();
void                 uploadReadyAppIcons();
void                 clearAppIconCache();

#endif
