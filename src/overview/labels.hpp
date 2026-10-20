#ifndef HYPRWINVIEW_OVERVIEW_LABELS_HPP
#define HYPRWINVIEW_OVERVIEW_LABELS_HPP

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/Color.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class COverviewLabels {
  public:
    struct STextTexture {
        SP<Render::ITexture> texture;
        Vector2D             size;
    };

    void         beginFrame();
    void         endFrame();
    STextTexture textureForLines(const std::vector<std::string>& lines, int fontSizePx,
                                 int maxWidthPx, const CHyprColor& color, const std::string& font);

  private:
    struct SEntry {
        STextTexture text;
        bool         used = false;
    };
    std::unordered_map<std::string, SEntry> cache;
};

#endif
