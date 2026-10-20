#include "labels.hpp"
#include "text.hpp"

#include <algorithm>
#include <cmath>
#include <hyprland/src/render/Renderer.hpp>

void COverviewLabels::beginFrame() {
    for (auto& [key, entry] : cache)
        entry.used = false;
}

void COverviewLabels::endFrame() {
    std::erase_if(cache, [](const auto& item) { return !item.second.used; });
}

COverviewLabels::STextTexture
COverviewLabels::textureForLines(const std::vector<std::string>& lines, int fontSizePx,
                                 int maxWidthPx, const CHyprColor& color, const std::string& font) {
    if (lines.empty() || fontSizePx <= 0 || maxWidthPx <= 0)
        return {};

    std::string key = std::to_string(font.size()) + ":" + font + "|" + std::to_string(fontSizePx) +
        "|" + std::to_string(maxWidthPx) + "|" + std::to_string((uint64_t)(color.r * 255.0)) + "," +
        std::to_string((uint64_t)(color.g * 255.0)) + "," +
        std::to_string((uint64_t)(color.b * 255.0)) + "," +
        std::to_string((uint64_t)(color.a * 255.0));
    for (const auto& line : lines)
        key += "|" + std::to_string(line.size()) + ":" + line;

    if (const auto IT = cache.find(key); IT != cache.end()) {
        IT->second.used = true;
        return IT->second.text;
    }

    cairo_surface_t* measureSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t*         measure        = cairo_create(measureSurface);
    cairo_select_font_face(measure, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(measure, fontSizePx);

    cairo_font_extents_t fontExtents;
    cairo_font_extents(measure, &fontExtents);

    std::vector<std::string> renderedLines;
    renderedLines.reserve(lines.size());
    double width = 1.0;
    for (const auto& line : lines) {
        auto renderedLine = Winview::elideTextToWidth(measure, line, maxWidthPx);
        width             = std::max(width, Winview::cairoTextWidth(measure, renderedLine));
        renderedLines.push_back(std::move(renderedLine));
    }

    const int textureWidth  = std::max(1, (int)std::ceil(std::min<double>(width, maxWidthPx)));
    const int lineHeight    = std::max(1, (int)std::ceil(fontExtents.height));
    const int textureHeight = std::max(1, lineHeight * (int)renderedLines.size());

    cairo_destroy(measure);
    cairo_surface_destroy(measureSurface);

    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, textureWidth, textureHeight);
    cairo_t* cr = cairo_create(surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_select_font_face(cr, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, fontSizePx);
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

    for (size_t i = 0; i < renderedLines.size(); ++i) {
        cairo_move_to(cr, 0.0, fontExtents.ascent + static_cast<double>(i) * lineHeight);
        cairo_show_text(cr, renderedLines[i].c_str());
    }

    cairo_destroy(cr);

    auto texture = g_pHyprRenderer->createTexture(surface);
    cairo_surface_destroy(surface);

    if (!texture)
        return {};

    const Vector2D size{(double)textureWidth, (double)textureHeight};
    cache.emplace(key, SEntry{.text = {.texture = texture, .size = size}, .used = true});
    return {.texture = texture, .size = size};
}
