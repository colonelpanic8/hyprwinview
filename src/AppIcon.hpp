#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Texture.hpp>

SP<Render::ITexture> appIconTextureForWindow(PHLWINDOW window, int sizePx);
void                 clearAppIconCache();
