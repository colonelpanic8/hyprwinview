#ifndef HYPRWINVIEW_APP_ICON_HPP
#define HYPRWINVIEW_APP_ICON_HPP

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Texture.hpp>

int                  appIconPixelSize(double monitorScale);
SP<Render::ITexture> appIconTextureForWindow(const PHLWINDOW& window, int sizePx);
void                 initializeAppIconCache();
void                 uploadReadyAppIcons();
// Re-resolves every icon so newly installed desktop entries and icon files are found.
void invalidateAppIconCache();
void clearAppIconCache();

#endif
