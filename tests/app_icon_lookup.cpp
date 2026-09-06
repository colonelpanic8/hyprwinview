#include "app_icon_lookup.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;
using AppIconLookup::SConfig;
using AppIconLookup::findAppIconPath;

static void writeFile(const fs::path& path, const std::string& contents = "") {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << contents;
}

static void expectPath(const std::optional<std::string>& result, const fs::path& expected) {
    if (result != expected.string())
        throw std::runtime_error("expected " + expected.string() + ", got " +
                                 result.value_or("missing"));
}

static void expectMissing(const std::optional<std::string>& result) {
    if (result)
        throw std::runtime_error("expected missing icon, got " + *result);
}

int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    const fs::path ROOT   = fs::absolute(argv[1]);
    const auto     DATA   = ROOT / "data";
    const auto     SYSTEM = ROOT / "system";
    const auto     CONFIG = ROOT / "config";
    setenv("HOME", ROOT.c_str(), 1);
    setenv("XDG_DATA_HOME", DATA.c_str(), 1);
    setenv("XDG_DATA_DIRS", SYSTEM.c_str(), 1);
    setenv("XDG_CONFIG_HOME", CONFIG.c_str(), 1);
    setenv("XDG_CURRENT_DESKTOP", "", 1);
    setenv("QT_QPA_PLATFORMTHEME", "", 1);

    const std::string INDEX =
        "[Icon Theme]\nDirectories=16x16/apps,48x48/apps,96x96/apps,scalable/apps\n"
        "[16x16/apps]\nSize=16\nType=Fixed\n"
        "[48x48/apps]\nSize=48\nType=Fixed\n"
        "[96x96/apps]\nSize=96\nType=Fixed\n"
        "[scalable/apps]\nSize=48\nType=Scalable\nMinSize=16\nMaxSize=256\n";
    const auto THEME = DATA / "icons/test";
    writeFile(THEME / "index.theme", INDEX);
    writeFile(SYSTEM / "icons/test/index.theme", INDEX);
    writeFile(THEME / "16x16/apps/raster.png");
    writeFile(THEME / "96x96/apps/raster.png");
    writeFile(THEME / "48x48/apps/tie.png");
    writeFile(THEME / "48x48/apps/tie.svg");
    writeFile(SYSTEM / "icons/test/48x48/apps/tie.svg");
    writeFile(THEME / "scalable/apps/vector.svg");
    const SConfig THEMED{.theme = "test", .overrides = ""};
    expectPath(findAppIconPath({"raster"}, 48, THEMED), THEME / "96x96/apps/raster.png");
    expectPath(findAppIconPath({"tie"}, 48, THEMED), THEME / "48x48/apps/tie.svg");
    expectPath(findAppIconPath({"vector"}, 72, THEMED), THEME / "scalable/apps/vector.svg");

    writeFile(DATA / "icons/parent/index.theme", "[Icon Theme]\nInherits=child\n");
    writeFile(DATA / "icons/child/index.theme", INDEX + "\n[Icon Theme]\nInherits=parent\n");
    writeFile(DATA / "icons/child/48x48/apps/inherited.png");
    writeFile(DATA / "icons/hicolor/index.theme", INDEX);
    writeFile(DATA / "icons/hicolor/48x48/apps/standard.png");
    const SConfig INHERITED{.theme = "parent", .overrides = ""};
    expectPath(findAppIconPath({"inherited"}, 48, INHERITED),
               DATA / "icons/child/48x48/apps/inherited.png");
    expectPath(findAppIconPath({"standard"}, 48, INHERITED),
               DATA / "icons/hicolor/48x48/apps/standard.png");
    expectMissing(findAppIconPath({"missing"}, 48, INHERITED));

    const auto LEGACY = DATA / "icons/unindexed";
    writeFile(LEGACY / "32x32/apps/legacy.png");
    writeFile(LEGACY / "64x64/apps/legacy.png");
    writeFile(LEGACY / "48x48@2/apps/scaled.png");
    writeFile(DATA / "pixmaps/other.SVG");
    fs::create_symlink(DATA / "pixmaps/other.SVG", DATA / "pixmaps/linked.svg");
    const SConfig FALLBACK{.theme = "", .source = "legacy", .overrides = ""};
    AppIconLookup::clearCache();
    expectPath(findAppIconPath({"legacy"}, 32, FALLBACK), LEGACY / "32x32/apps/legacy.png");
    expectPath(findAppIconPath({"legacy.png"}, 48, FALLBACK), LEGACY / "64x64/apps/legacy.png");
    expectPath(findAppIconPath({"scaled"}, 96, FALLBACK), LEGACY / "48x48@2/apps/scaled.png");
    expectPath(findAppIconPath({"other"}, 48, FALLBACK), DATA / "pixmaps/other.SVG");
    expectPath(findAppIconPath({"linked"}, 48, FALLBACK), DATA / "pixmaps/linked.svg");
    expectMissing(findAppIconPath({"late"}, 48, FALLBACK));
    writeFile(DATA / "pixmaps/late.svg");
    expectMissing(findAppIconPath({"late"}, 48, FALLBACK));
    AppIconLookup::clearCache();
    expectPath(findAppIconPath({"late"}, 48, FALLBACK), DATA / "pixmaps/late.svg");

    writeFile(DATA / "applications/editor.desktop",
              "[Desktop Entry]\nIcon=tie\nStartupWMClass=EditorClass\n");
    AppIconLookup::clearCache();
    expectPath(findAppIconPath({"editor"}, 48, THEMED), THEME / "48x48/apps/tie.svg");
    expectPath(findAppIconPath({"EditorClass"}, 48, THEMED), THEME / "48x48/apps/tie.svg");
    expectPath(findAppIconPath({"unknown", "editor"}, 48, THEMED), THEME / "48x48/apps/tie.svg");
    expectPath(findAppIconPath({"editor"}, 48, {.theme = "test", .overrides = "EDITOR = vector"}),
               THEME / "scalable/apps/vector.svg");
    expectPath(findAppIconPath(
                   {"editor"}, 48,
                   {.theme = "", .overrides = "editor=" + (DATA / "pixmaps/other.SVG").string()}),
               DATA / "pixmaps/other.SVG");

    writeFile(CONFIG / "gtk-3.0/settings.ini", "[Settings]\ngtk-icon-theme-name=test\n");
    AppIconLookup::clearCache();
    expectPath(findAppIconPath({"tie"}, 48, {.theme = "", .source = "gtk", .overrides = ""}),
               THEME / "48x48/apps/tie.svg");
    writeFile(CONFIG / "gtk-3.0/settings.ini", "[Settings]\ngtk-icon-theme-name=child\n");
    writeFile(DATA / "icons/child/48x48/apps/tie.svg");
    expectPath(findAppIconPath({"tie"}, 48, {.theme = "", .source = "gtk", .overrides = ""}),
               THEME / "48x48/apps/tie.svg");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    expectPath(findAppIconPath({"tie"}, 48, {.theme = "", .source = "gtk", .overrides = ""}),
               DATA / "icons/child/48x48/apps/tie.svg");
    expectPath(findAppIconPath({"tie"}, 48, THEMED), THEME / "48x48/apps/tie.svg");
    expectPath(findAppIconPath({"tie"}, 48, {.theme = "", .source = "gtk", .overrides = ""}),
               DATA / "icons/child/48x48/apps/tie.svg");
    AppIconLookup::clearCache();
    std::cout << "Icon lookup tests passed\n";
}
