#include "overview/text.hpp"
#include "overview/grouping.hpp"
#include "workspace/layer.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

static void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

static void testFilterDeletion() {
    std::string query = "aé中😀";
    for (const auto* expected : {"aé中", "aé", "a", ""}) {
        expect(Winview::eraseLastCodepoint(query), "nonempty query did not change");
        expect(query == expected, "backspace split a UTF-8 codepoint");
    }
    expect(!Winview::eraseLastCodepoint(query), "backspace changed an empty query");
}

static void testTitleElision() {
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    auto* cr      = cairo_create(surface);
    cairo_set_font_size(cr, 16);
    for (const std::string title : {"日本語の長いウィンドウタイトル", "Résumé 😀 terminal title"}) {
        for (int width = 1; width < 180; ++width) {
            const auto text = Winview::elideTextToWidth(cr, title, width);
            expect(Winview::cairoTextWidth(cr, text) <= width, "elided title exceeds its width");
            expect(cairo_status(cr) == CAIRO_STATUS_SUCCESS, "elision produced invalid UTF-8");
        }
    }
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

static void testLayerArguments() {
    expect(HT::targetLayer("2", 0, 3, false) == 2, "absolute layer selection failed");
    expect(HT::targetLayer("+1", 1, 3, false) == 2, "relative forward selection failed");
    expect(HT::targetLayer("-1", 1, 3, false) == 0, "relative backward selection failed");
    expect(HT::targetLayer(" +1 ", 0, 3, false) == 1, "whitespace was not trimmed");
    expect(HT::targetLayer("+1", 2, 3, true) == 0, "forward wrap failed");
    expect(HT::targetLayer("-1", 0, 3, true) == 2, "backward wrap failed");
    expect(HT::targetLayer("+1", 2, 3, false) == 3, "nonwrapping boundary wrapped");
    expect(HT::targetLayer("-4", 0, 3, true) == 2, "multiple-layer wrap failed");
    expect(HT::targetLayer("+2147483647", 2, 3, true) == 0, "relative sum overflowed");
    expect(HT::targetLayer(std::to_string(std::numeric_limits<int>::min()), 0, 3, true) == 1,
           "minimum integer wrap failed");
    for (const auto* invalid :
         {"", " ", "+", "-", "+-1", "++1", "abc", "1junk", "1 2", "2147483648", "-2147483649"})
        expect(!HT::targetLayer(invalid, 0, 3, true), "invalid layer argument accepted");
    expect(!HT::targetLayer("+1", 0, 0, true), "zero layers accepted");
    expect(!HT::targetLayer("+1", 0, -1, true), "negative layers accepted");
}

int main() {
    testFilterDeletion();
    testTitleElision();
    expect(Winview::stableGroupOrder({}).empty(), "empty grouping is not empty");
    expect(Winview::stableGroupOrder({"B", "A", "B", "C", "A"}) ==
               std::vector<size_t>{0, 2, 1, 4, 3},
           "grouping changed first-seen group or within-group order");
    testLayerArguments();
    std::cout << "Overview behavior tests passed\n";
}
