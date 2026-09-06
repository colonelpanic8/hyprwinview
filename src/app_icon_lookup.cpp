#include "app_icon_lookup.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace AppIconLookup {
    std::string lowercase(std::string value) {
        std::ranges::transform(value, value.begin(),
                               [](unsigned char c) { return std::tolower(c); });
        return value;
    }

    namespace {
        namespace fs = std::filesystem;

        struct SDesktopEntry {
            std::string id;
            std::string name;
            std::string startupWMClass;
            std::string icon;
        };

        struct SIconThemeDirectory {
            std::string name;
            std::string type      = "Threshold";
            int         size      = 0;
            int         scale     = 1;
            int         minSize   = 0;
            int         maxSize   = 0;
            int         threshold = 2;
        };

        struct SIconTheme {
            std::vector<fs::path>            roots;
            std::vector<std::string>         inherits;
            std::vector<SIconThemeDirectory> directories;
        };

        using SIniData = std::map<std::string, std::map<std::string, std::string>>;

        thread_local std::unordered_map<std::string, std::optional<SDesktopEntry>>
            g_desktopEntryCache;
        thread_local std::unordered_map<std::string, std::optional<std::string>> g_iconPathCache;
        thread_local std::unordered_map<std::string, std::optional<SIconTheme>>  g_iconThemeCache;
        thread_local std::optional<std::vector<SDesktopEntry>>                   g_desktopEntries;
        thread_local std::optional<std::unordered_map<std::string, std::vector<fs::path>>>
            g_legacyIconIndex;

        struct SThemeNamesCache {
            std::string                           theme;
            std::string                           source;
            std::vector<std::string>              names;
            std::chrono::steady_clock::time_point refreshedAt;
        };

        thread_local std::optional<SThemeNamesCache> g_themeNamesCache;

        // Prefer a larger raster over upscaling a smaller one whenever the theme
        // provides one; the rendered texture is still capped by app_icon_size.
        constexpr int        UNDERSIZED_RASTER_PENALTY = 1'000'000;

        thread_local SConfig g_config;

        std::string          trim(std::string value) {
            const auto BEGIN =
                std::ranges::find_if_not(value, [](unsigned char c) { return std::isspace(c); });
            const auto END = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
                                 return std::isspace(c);
                             }).base();

            if (BEGIN >= END)
                return "";

            return std::string(BEGIN, END);
        }

        std::vector<std::string> splitList(const std::string& value, char delimiter) {
            std::vector<std::string> result;
            std::stringstream        stream(value);
            std::string              token;
            while (std::getline(stream, token, delimiter)) {
                token = trim(token);
                if (!token.empty())
                    result.push_back(token);
            }

            return result;
        }

        int parseInt(const std::string& value, int fallback = 0) {
            try {
                size_t    consumed = 0;
                const int result   = std::stoi(value, &consumed);
                return consumed == value.size() ? result : fallback;
            } catch (...) { return fallback; }
        }

        std::vector<std::string> splitEnvPaths(const char*                     value,
                                               const std::vector<std::string>& defaults) {
            if (!value || std::string_view(value).empty())
                return defaults;

            std::vector<std::string> paths;
            std::stringstream        stream(value);
            std::string              path;
            while (std::getline(stream, path, ':')) {
                path = trim(path);
                if (!path.empty())
                    paths.push_back(path);
            }

            return paths.empty() ? defaults : paths;
        }

        fs::path configHomePath() {
            const auto XDG_CONFIG_HOME = std::getenv("XDG_CONFIG_HOME");
            if (XDG_CONFIG_HOME && *XDG_CONFIG_HOME)
                return XDG_CONFIG_HOME;

            const auto HOME = std::getenv("HOME");
            return HOME && *HOME ? fs::path(HOME) / ".config" : fs::path{};
        }

        std::vector<fs::path> dataDirs() {
            std::vector<fs::path> dirs;

            const auto            HOME          = std::getenv("HOME");
            const auto            XDG_DATA_HOME = std::getenv("XDG_DATA_HOME");
            if (XDG_DATA_HOME && *XDG_DATA_HOME)
                dirs.emplace_back(XDG_DATA_HOME);
            else if (HOME && *HOME)
                dirs.emplace_back(fs::path(HOME) / ".local/share");

            for (const auto& path :
                 splitEnvPaths(std::getenv("XDG_DATA_DIRS"), {"/usr/local/share", "/usr/share"}))
                dirs.emplace_back(path);

            return dirs;
        }

        std::vector<fs::path> iconThemeBaseDirs() {
            auto                  dirs = dataDirs();
            std::vector<fs::path> bases;

            const auto            HOME = std::getenv("HOME");
            if (HOME && *HOME)
                bases.emplace_back(fs::path(HOME) / ".icons");

            for (const auto& dir : dirs)
                bases.emplace_back(dir / "icons");

            return bases;
        }

        std::vector<fs::path> iconRoots() {
            auto                  dirs = dataDirs();
            std::vector<fs::path> roots;

            for (const auto& dir : iconThemeBaseDirs())
                roots.emplace_back(dir);

            for (const auto& dir : dirs) {
                roots.emplace_back(dir / "pixmaps");
            }

            return roots;
        }

        bool hasImageExtension(const fs::path& path) {
            const auto EXT = lowercase(path.extension().string());
            return EXT == ".png" || EXT == ".svg";
        }

        SIniData parseIniFile(const fs::path& path) {
            std::ifstream file(path);
            if (!file)
                return {};

            SIniData    data;
            std::string section;
            std::string line;
            while (std::getline(file, line)) {
                line = trim(line);
                if (line.empty() || line.starts_with('#') || line.starts_with(';'))
                    continue;

                if (line.starts_with('[') && line.ends_with(']')) {
                    section = line.substr(1, line.size() - 2);
                    continue;
                }

                const auto EQUAL = line.find('=');
                if (EQUAL == std::string::npos)
                    continue;

                data[section][trim(line.substr(0, EQUAL))] = trim(line.substr(EQUAL + 1));
            }

            return data;
        }

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        std::optional<std::string> iniValue(const fs::path& path, const std::string& section,
                                            const std::string& key) {
            const auto DATA    = parseIniFile(path);
            const auto SECTION = DATA.find(section);
            if (SECTION == DATA.end())
                return std::nullopt;

            const auto VALUE = SECTION->second.find(key);
            if (VALUE == SECTION->second.end() || VALUE->second.empty())
                return std::nullopt;

            return VALUE->second;
        }

        std::optional<std::string> xsettingsIconThemeName(const fs::path& path) {
            std::ifstream file(path);
            if (!file)
                return std::nullopt;

            std::string line;
            while (std::getline(file, line)) {
                line = trim(line);
                if (line.empty() || line.starts_with('#') || !line.starts_with("Net/IconThemeName"))
                    continue;

                auto value = trim(line.substr(std::string_view("Net/IconThemeName").size()));
                if (value.size() >= 2 &&
                    ((value.front() == '"' && value.back() == '"') ||
                     (value.front() == '\'' && value.back() == '\'')))
                    value = value.substr(1, value.size() - 2);

                if (!value.empty())
                    return value;
            }

            return std::nullopt;
        }

        std::optional<SDesktopEntry> parseDesktopFile(const fs::path& path) {
            std::ifstream file(path);
            if (!file)
                return std::nullopt;

            SDesktopEntry entry;
            entry.id = path.stem().string();

            bool        inDesktopEntry = false;
            std::string line;
            while (std::getline(file, line)) {
                line = trim(line);
                if (line.empty() || line.starts_with('#'))
                    continue;

                if (line.starts_with('[') && line.ends_with(']')) {
                    inDesktopEntry = line == "[Desktop Entry]";
                    continue;
                }

                if (!inDesktopEntry)
                    continue;

                const auto EQUAL = line.find('=');
                if (EQUAL == std::string::npos)
                    continue;

                const auto KEY = line.substr(0, EQUAL);
                const auto VAL = trim(line.substr(EQUAL + 1));

                if (KEY == "Name")
                    entry.name = VAL;
                else if (KEY == "StartupWMClass")
                    entry.startupWMClass = VAL;
                else if (KEY == "Icon")
                    entry.icon = VAL;
            }

            if (entry.icon.empty())
                return std::nullopt;

            return entry;
        }

        const std::vector<SDesktopEntry>& desktopEntries() {
            if (g_desktopEntries)
                return *g_desktopEntries;

            std::vector<SDesktopEntry> entries;

            for (const auto& dir : dataDirs()) {
                const auto      APPDIR = dir / "applications";
                std::error_code ec;
                if (!fs::exists(APPDIR, ec))
                    continue;

                for (fs::recursive_directory_iterator
                         it(APPDIR, fs::directory_options::skip_permission_denied, ec),
                     end;
                     it != end; it.increment(ec)) {
                    if (ec || !it->is_regular_file(ec) || it->path().extension() != ".desktop")
                        continue;

                    if (auto entry = parseDesktopFile(it->path()))
                        entries.push_back(*entry);
                }
            }

            g_desktopEntries = std::move(entries);
            return *g_desktopEntries;
        }

        std::optional<SDesktopEntry> findDesktopEntry(const std::string& appId) {
            const auto KEY = lowercase(appId);
            if (const auto IT = g_desktopEntryCache.find(KEY); IT != g_desktopEntryCache.end())
                return IT->second;

            for (const auto& dir : dataDirs()) {
                for (const auto& candidate : {appId, lowercase(appId)}) {
                    const auto      PATH = dir / "applications" / (candidate + ".desktop");
                    std::error_code ec;
                    if (!fs::is_regular_file(PATH, ec))
                        continue;

                    auto entry = parseDesktopFile(PATH);
                    g_desktopEntryCache.emplace(KEY, entry);
                    return entry;
                }
            }

            for (const auto& entry : desktopEntries()) {
                if (lowercase(entry.id) == KEY || lowercase(entry.startupWMClass) == KEY) {
                    g_desktopEntryCache.emplace(KEY, entry);
                    return entry;
                }
            }

            g_desktopEntryCache.emplace(KEY, std::nullopt);
            return std::nullopt;
        }

        std::string iconLookupName(const std::string& iconName) {
            const fs::path ICON(iconName);
            const auto     EXT = lowercase(ICON.extension().string());
            if (EXT != ".png" && EXT != ".svg")
                return iconName;

            auto withoutExtension = ICON;
            withoutExtension.replace_extension("");
            return withoutExtension.string();
        }

        std::vector<std::string> iconFileNames(const std::string& iconName) {
            const fs::path ICON(iconName);
            const auto     EXT = lowercase(ICON.extension().string());
            if ((EXT == ".png" || EXT == ".svg") && ICON.filename() == ICON)
                return {iconName};

            return {iconName + ".svg", iconName + ".png"};
        }

        int iconDirectoryDistance(const SIconThemeDirectory& dir, int sizePx) {
            const auto TYPE      = lowercase(dir.type);
            const int  SCALE     = std::max(1, dir.scale);
            const int  SIZE      = dir.size * SCALE;
            const int  MIN_SIZE  = dir.minSize * SCALE;
            const int  MAX_SIZE  = dir.maxSize * SCALE;
            const int  THRESHOLD = dir.threshold * SCALE;

            if (TYPE == "scalable") {
                if (sizePx >= MIN_SIZE && sizePx <= MAX_SIZE)
                    return 0;
                return std::min(std::abs(sizePx - MIN_SIZE), std::abs(sizePx - MAX_SIZE));
            }

            if (TYPE == "threshold") {
                if (std::abs(sizePx - SIZE) <= THRESHOLD)
                    return 0;
                return std::abs(sizePx - SIZE) - THRESHOLD;
            }

            return std::abs(sizePx - SIZE);
        }

        int iconDirectoryNominalSize(const SIconThemeDirectory& dir) {
            return dir.size * std::max(1, dir.scale);
        }

        int fallbackSizeFromPath(const fs::path& path) {
            const auto PARENT = path.parent_path();
            for (auto it = PARENT.end(); it != PARENT.begin();) {
                --it;
                const auto PART = it->string();
                const auto X    = PART.find('x');
                if (X != std::string::npos) {
                    const auto SCALE_MARKER = PART.find('@', X + 1);
                    const int  SIZE =
                        std::max(parseInt(PART.substr(0, X), 0),
                                 parseInt(PART.substr(X + 1,
                                                      SCALE_MARKER == std::string::npos ?
                                                          std::string::npos :
                                                          SCALE_MARKER - X - 1),
                                          0));
                    const int SCALE = SCALE_MARKER == std::string::npos ?
                        1 :
                        parseInt(PART.substr(SCALE_MARKER + 1), 1);
                    return SIZE * std::max(1, SCALE);
                }

                const auto SIZE = parseInt(PART, 0);
                if (SIZE > 0)
                    return SIZE;
            }

            return 0;
        }

        int iconFileTieBreakScore(const fs::path& path, int nominalSize, int sizePx,
                                  bool scalable) {
            const auto EXT        = lowercase(path.extension().string());
            const bool SVG        = EXT == ".svg";
            const bool UNDERSIZED = !scalable && nominalSize > 0 && nominalSize < sizePx;

            return (UNDERSIZED ? UNDERSIZED_RASTER_PENALTY : 0) + (SVG ? 0 : 10) +
                (nominalSize >= sizePx ? 0 : 50);
        }

        const std::optional<SIconTheme>& loadIconTheme(const std::string& themeName) {
            const auto& CACHE_KEY = themeName;
            if (const auto IT = g_iconThemeCache.find(CACHE_KEY); IT != g_iconThemeCache.end())
                return IT->second;

            SIconTheme              theme;
            std::optional<fs::path> indexPath;

            for (const auto& base : iconThemeBaseDirs()) {
                const auto      ROOT = base / themeName;
                std::error_code ec;
                if (!fs::is_directory(ROOT, ec))
                    continue;

                theme.roots.push_back(ROOT);
                if (!indexPath && fs::is_regular_file(ROOT / "index.theme", ec))
                    indexPath = ROOT / "index.theme";
            }

            if (theme.roots.empty()) {
                return g_iconThemeCache.emplace(CACHE_KEY, std::nullopt).first->second;
            }

            if (indexPath) {
                const auto DATA = parseIniFile(*indexPath);
                if (const auto IT = DATA.find("Icon Theme"); IT != DATA.end()) {
                    if (const auto INHERITS = IT->second.find("Inherits");
                        INHERITS != IT->second.end())
                        theme.inherits = splitList(INHERITS->second, ',');

                    if (const auto DIRS = IT->second.find("Directories");
                        DIRS != IT->second.end()) {
                        for (const auto& dirName : splitList(DIRS->second, ',')) {
                            const auto DIR_SECTION = DATA.find(dirName);
                            if (DIR_SECTION == DATA.end())
                                continue;

                            SIconThemeDirectory dir;
                            dir.name = dirName;
                            if (const auto SIZE = DIR_SECTION->second.find("Size");
                                SIZE != DIR_SECTION->second.end())
                                dir.size = parseInt(SIZE->second, 0);
                            if (const auto SCALE = DIR_SECTION->second.find("Scale");
                                SCALE != DIR_SECTION->second.end())
                                dir.scale = std::max(1, parseInt(SCALE->second, 1));
                            if (const auto TYPE = DIR_SECTION->second.find("Type");
                                TYPE != DIR_SECTION->second.end())
                                dir.type = TYPE->second;
                            if (const auto MIN = DIR_SECTION->second.find("MinSize");
                                MIN != DIR_SECTION->second.end())
                                dir.minSize = parseInt(MIN->second, dir.size);
                            else
                                dir.minSize = dir.size;
                            if (const auto MAX = DIR_SECTION->second.find("MaxSize");
                                MAX != DIR_SECTION->second.end())
                                dir.maxSize = parseInt(MAX->second, dir.size);
                            else
                                dir.maxSize = dir.size;
                            if (const auto THRESHOLD = DIR_SECTION->second.find("Threshold");
                                THRESHOLD != DIR_SECTION->second.end())
                                dir.threshold = parseInt(THRESHOLD->second, 2);

                            if (iconDirectoryNominalSize(dir) > 0 ||
                                lowercase(dir.type) == "scalable")
                                theme.directories.push_back(dir);
                        }
                    }
                }
            }

            return g_iconThemeCache.emplace(CACHE_KEY, std::move(theme)).first->second;
        }

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        std::optional<fs::path> findIconInTheme(const std::string& iconName,
                                                const std::string& themeName, int sizePx,
                                                std::unordered_set<std::string>& visited) {
            const auto& VISIT_KEY = themeName;
            if (VISIT_KEY.empty() || visited.contains(VISIT_KEY))
                return std::nullopt;

            visited.insert(VISIT_KEY);

            const auto& THEME = loadIconTheme(themeName);
            if (!THEME)
                return std::nullopt;

            std::optional<fs::path> bestPath;
            int                     bestScore  = std::numeric_limits<int>::max();
            const auto              FILE_NAMES = iconFileNames(iconName);
            for (const auto& dir : THEME->directories) {
                const bool SCALABLE = lowercase(dir.type) == "scalable";
                const int  DISTANCE = iconDirectoryDistance(dir, sizePx);
                const int  NOMINAL  = SCALABLE ? sizePx : iconDirectoryNominalSize(dir);

                for (const auto& root : THEME->roots) {
                    for (const auto& fileName : FILE_NAMES) {
                        const int SCORE = DISTANCE * 1000 +
                            iconFileTieBreakScore(fileName, NOMINAL, sizePx, SCALABLE);
                        if (SCORE >= bestScore)
                            continue;

                        const auto      PATH = root / dir.name / fileName;
                        std::error_code ec;
                        if (!fs::is_regular_file(PATH, ec))
                            continue;

                        bestScore = SCORE;
                        bestPath  = PATH;
                        if (bestScore == 0)
                            return bestPath;
                    }
                }
            }

            if (bestPath)
                return bestPath;

            for (const auto& inherited : THEME->inherits) {
                if (auto path = findIconInTheme(iconName, inherited, sizePx, visited))
                    return path;
            }

            if (VISIT_KEY != "hicolor")
                return findIconInTheme(iconName, "hicolor", sizePx, visited);

            return std::nullopt;
        }

        std::optional<std::string> gtkIconThemeName() {
            const auto CONFIG_HOME = configHomePath();
            if (CONFIG_HOME.empty())
                return std::nullopt;

            if (auto value = xsettingsIconThemeName(CONFIG_HOME / "xsettingsd/xsettingsd.conf"))
                return value;

            const auto HOME = std::getenv("HOME");
            if (HOME && *HOME) {
                if (auto value = xsettingsIconThemeName(fs::path(HOME) / ".xsettingsd"))
                    return value;
            }

            for (const auto& path :
                 {CONFIG_HOME / "gtk-4.0/settings.ini", CONFIG_HOME / "gtk-3.0/settings.ini"}) {
                if (auto value = iniValue(path, "Settings", "gtk-icon-theme-name"))
                    return value;
            }

            return std::nullopt;
        }

        std::optional<std::string> qtIconThemeName() {
            const auto CONFIG_HOME = configHomePath();
            if (CONFIG_HOME.empty())
                return std::nullopt;

            const auto        QT_PLATFORM_THEME = std::getenv("QT_QPA_PLATFORMTHEME");
            const std::string PLATFORM_THEME =
                lowercase(QT_PLATFORM_THEME ? QT_PLATFORM_THEME : "");
            std::vector<fs::path> paths;
            if (PLATFORM_THEME.contains("qt6ct"))
                paths = {CONFIG_HOME / "qt6ct/qt6ct.conf", CONFIG_HOME / "qt5ct/qt5ct.conf",
                         CONFIG_HOME / "kdeglobals"};
            else if (PLATFORM_THEME.contains("qt5ct"))
                paths = {CONFIG_HOME / "qt5ct/qt5ct.conf", CONFIG_HOME / "qt6ct/qt6ct.conf",
                         CONFIG_HOME / "kdeglobals"};
            else
                paths = {CONFIG_HOME / "kdeglobals", CONFIG_HOME / "qt6ct/qt6ct.conf",
                         CONFIG_HOME / "qt5ct/qt5ct.conf"};

            for (const auto& path : paths) {
                if (path.filename() == "kdeglobals") {
                    if (auto value = iniValue(path, "Icons", "Theme"))
                        return value;
                } else if (auto value = iniValue(path, "Appearance", "icon_theme"))
                    return value;
            }

            return std::nullopt;
        }

        void appendUnique(std::vector<std::string>&         values,
                          const std::optional<std::string>& value) {
            if (!value || value->empty())
                return;

            if (std::ranges::find_if(values, [&](const auto& existing) {
                    return lowercase(existing) == lowercase(*value);
                }) == values.end())
                values.push_back(*value);
        }

        bool themeLookupDisabled() {
            if (!g_config.theme.empty())
                return false;

            const std::string SOURCE = lowercase(g_config.source);
            return SOURCE == "none" || SOURCE == "legacy";
        }

        std::vector<std::string> readConfiguredThemeNames() {
            std::vector<std::string> themes;

            appendUnique(themes, g_config.theme);
            if (!themes.empty())
                return themes;

            if (themeLookupDisabled())
                return themes;

            const std::string SOURCE = lowercase(g_config.source);
            if (SOURCE == "gtk") {
                appendUnique(themes, gtkIconThemeName());
                return themes;
            }

            if (SOURCE == "qt") {
                appendUnique(themes, qtIconThemeName());
                return themes;
            }

            const auto        DESKTOP_ENV     = std::getenv("XDG_CURRENT_DESKTOP");
            const auto        PLATFORM_ENV    = std::getenv("QT_QPA_PLATFORMTHEME");
            const std::string CURRENT_DESKTOP = lowercase(DESKTOP_ENV ? DESKTOP_ENV : "");
            const std::string PLATFORM_THEME  = lowercase(PLATFORM_ENV ? PLATFORM_ENV : "");
            const bool        QT_FIRST        = CURRENT_DESKTOP.contains("kde") ||
                CURRENT_DESKTOP.contains("lxqt") || PLATFORM_THEME.contains("qt") ||
                PLATFORM_THEME.contains("kde");

            if (QT_FIRST) {
                appendUnique(themes, qtIconThemeName());
                appendUnique(themes, gtkIconThemeName());
            } else {
                appendUnique(themes, gtkIconThemeName());
                appendUnique(themes, qtIconThemeName());
            }

            return themes;
        }

        const std::vector<std::string>& configuredThemeNames() {
            const auto THEME  = g_config.theme;
            const auto SOURCE = g_config.source;
            const auto NOW    = std::chrono::steady_clock::now();
            if (!g_themeNamesCache || g_themeNamesCache->theme != THEME ||
                g_themeNamesCache->source != SOURCE ||
                NOW - g_themeNamesCache->refreshedAt >= std::chrono::seconds(1)) {
                g_themeNamesCache =
                    SThemeNamesCache{THEME, SOURCE, readConfiguredThemeNames(), NOW};
            }
            return g_themeNamesCache->names;
        }

        std::string themeCacheKeyPart(const std::vector<std::string>& themes) {
            std::string key;
            for (const auto& theme : themes) {
                if (!key.empty())
                    key += ",";
                key += theme;
            }

            return key;
        }

        std::optional<fs::path> findIconByTheme(const std::string& iconName, int sizePx,
                                                const std::vector<std::string>& themes) {
            const auto LOOKUP_NAME = iconLookupName(iconName);
            for (const auto& themeName : themes) {
                std::unordered_set<std::string> visited;
                if (auto path = findIconInTheme(LOOKUP_NAME, themeName, sizePx, visited))
                    return path;
            }

            std::unordered_set<std::string> visited;
            return findIconInTheme(LOOKUP_NAME, "hicolor", sizePx, visited);
        }

        const std::unordered_map<std::string, std::vector<fs::path>>& legacyIconIndex() {
            if (g_legacyIconIndex)
                return *g_legacyIconIndex;

            std::unordered_map<std::string, std::vector<fs::path>> index;
            for (const auto& root : iconRoots()) {
                std::error_code ec;
                for (fs::recursive_directory_iterator
                         it(root, fs::directory_options::skip_permission_denied, ec),
                     end;
                     it != end; it.increment(ec)) {
                    if (ec || !hasImageExtension(it->path()) || !it->is_regular_file(ec))
                        continue;
                    index[it->path().stem().string()].push_back(it->path());
                }
            }
            g_legacyIconIndex = std::move(index);
            return *g_legacyIconIndex;
        }

        std::optional<std::string> findIconByLegacySearch(const std::string& iconName, int sizePx) {
            const auto& INDEX = legacyIconIndex();
            const auto  MATCH = INDEX.find(iconLookupName(iconName));
            if (MATCH == INDEX.end())
                return std::nullopt;

            std::optional<fs::path> bestPath;
            int                     bestScore = std::numeric_limits<int>::max();
            for (const auto& path : MATCH->second) {
                const int NOMINAL  = fallbackSizeFromPath(path);
                const int DISTANCE = NOMINAL > 0 ? std::abs(NOMINAL - sizePx) : sizePx;
                const int SCORE    = DISTANCE * 1000 +
                    iconFileTieBreakScore(path, NOMINAL, sizePx,
                                          lowercase(path.extension().string()) == ".svg");
                if (SCORE < bestScore) {
                    bestScore = SCORE;
                    bestPath  = path;
                }
            }
            return bestPath ? std::optional<std::string>{bestPath->string()} : std::nullopt;
        }

        std::optional<std::string> findIconPath(const std::string& iconName, int sizePx) {
            if (iconName.empty())
                return std::nullopt;

            const auto& THEMES    = configuredThemeNames();
            const auto  CACHE_KEY = iconName + ":" + std::to_string(sizePx) + ":" + g_config.theme +
                ":" + g_config.source + ":" + themeCacheKeyPart(THEMES);
            if (const auto IT = g_iconPathCache.find(CACHE_KEY); IT != g_iconPathCache.end())
                return IT->second;

            const fs::path  ICON(iconName);
            std::error_code ec;
            if (ICON.is_absolute() && fs::is_regular_file(ICON, ec)) {
                g_iconPathCache.emplace(CACHE_KEY, ICON.string());
                return ICON.string();
            }

            const auto THEME_RESULT = themeLookupDisabled() ?
                std::optional<fs::path>{} :
                findIconByTheme(iconName, sizePx, THEMES);
            const auto RESULT = THEME_RESULT ? std::optional<std::string>{THEME_RESULT->string()} :
                                               findIconByLegacySearch(iconName, sizePx);
            g_iconPathCache.emplace(CACHE_KEY, RESULT);
            return RESULT;
        }

        std::optional<std::string> iconOverrideForAppId(const std::string& appId) {
            const auto KEY = lowercase(appId);
            for (const auto& entry : splitList(g_config.overrides, ',')) {
                const auto EQUAL = entry.find('=');
                if (EQUAL == std::string::npos)
                    continue;

                if (lowercase(trim(entry.substr(0, EQUAL))) == KEY) {
                    auto ICON = trim(entry.substr(EQUAL + 1));
                    if (!ICON.empty())
                        return ICON;
                }
            }

            return std::nullopt;
        }

    }

    std::optional<std::string> findAppIconPath(const std::vector<std::string>& appIds, int sizePx,
                                               const SConfig& config) {
        g_config = config;
        for (const auto& appId : appIds) {
            if (auto overrideIcon = iconOverrideForAppId(appId)) {
                if (auto path = findIconPath(*overrideIcon, sizePx))
                    return path;
            }

            if (auto entry = findDesktopEntry(appId)) {
                if (auto path = findIconPath(entry->icon, sizePx))
                    return path;
            }

            if (auto path = findIconPath(appId, sizePx))
                return path;
        }

        return std::nullopt;
    }

    void clearCache() {
        g_iconPathCache.clear();
        g_iconThemeCache.clear();
        g_desktopEntryCache.clear();
        g_desktopEntries.reset();
        g_legacyIconIndex.reset();
        g_themeNamesCache.reset();
    }
}
