#include "app_icon_lookup.hpp"
#include "strings.hpp"

#include <algorithm>
#include <cctype>
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
    using Winview::lowercase;
    using Winview::trim;

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

        struct SResolverState {
            std::unordered_map<std::string, std::optional<SDesktopEntry>>         desktopEntryCache;
            std::unordered_map<std::string, std::optional<std::string>>           iconPathCache;
            std::unordered_map<std::string, std::optional<SIconTheme>>            iconThemeCache;
            std::optional<std::vector<SDesktopEntry>>                             desktopEntries;
            std::optional<std::unordered_map<std::string, std::vector<fs::path>>> legacyIconIndex;
            std::map<std::pair<std::string, std::string>, std::vector<std::string>> themeNames;
        };

        // Prefer a larger raster over upscaling a smaller one whenever the theme
        // provides one; the rendered texture is still capped by app_icon_size.
        constexpr int            UNDERSIZED_RASTER_PENALTY = 1'000'000;

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

        const std::vector<SDesktopEntry>* desktopEntries(SResolverState&        state,
                                                         const std::stop_token& stop) {
            if (state.desktopEntries)
                return &*state.desktopEntries;

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
                    if (stop.stop_requested())
                        return nullptr;
                    if (ec || !it->is_regular_file(ec) || it->path().extension() != ".desktop")
                        continue;

                    if (auto entry = parseDesktopFile(it->path()))
                        entries.push_back(*entry);
                }
            }

            state.desktopEntries = std::move(entries);
            return &*state.desktopEntries;
        }

        std::optional<SDesktopEntry> findDesktopEntry(SResolverState&        state,
                                                      const std::string&     appId,
                                                      const std::stop_token& stop) {
            auto&      cache = state.desktopEntryCache;
            const auto KEY   = lowercase(appId);
            if (const auto IT = cache.find(KEY); IT != cache.end())
                return IT->second;

            for (const auto& dir : dataDirs()) {
                for (const auto& candidate : {appId, lowercase(appId)}) {
                    const auto      PATH = dir / "applications" / (candidate + ".desktop");
                    std::error_code ec;
                    if (!fs::is_regular_file(PATH, ec))
                        continue;

                    auto entry = parseDesktopFile(PATH);
                    cache.emplace(KEY, entry);
                    return entry;
                }
            }

            const auto* entries = desktopEntries(state, stop);
            if (!entries)
                return std::nullopt;

            for (const auto& entry : *entries) {
                if (lowercase(entry.id) == KEY || lowercase(entry.startupWMClass) == KEY) {
                    cache.emplace(KEY, entry);
                    return entry;
                }
            }

            cache.emplace(KEY, std::nullopt);
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

        const std::optional<SIconTheme>& loadIconTheme(SResolverState&    state,
                                                       const std::string& themeName) {
            auto&       cache     = state.iconThemeCache;
            const auto& CACHE_KEY = themeName;
            if (const auto IT = cache.find(CACHE_KEY); IT != cache.end())
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
                return cache.emplace(CACHE_KEY, std::nullopt).first->second;
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

            return cache.emplace(CACHE_KEY, std::move(theme)).first->second;
        }

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        std::optional<fs::path> findIconInTheme(SResolverState& state, const std::string& iconName,
                                                const std::string& themeName, int sizePx,
                                                std::unordered_set<std::string>& visited) {
            const auto& VISIT_KEY = themeName;
            if (VISIT_KEY.empty() || visited.contains(VISIT_KEY))
                return std::nullopt;

            visited.insert(VISIT_KEY);

            const auto& THEME = loadIconTheme(state, themeName);
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
                if (auto path = findIconInTheme(state, iconName, inherited, sizePx, visited))
                    return path;
            }

            if (VISIT_KEY != "hicolor")
                return findIconInTheme(state, iconName, "hicolor", sizePx, visited);

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

        bool themeLookupDisabled(const SConfig& config) {
            if (!config.theme.empty())
                return false;

            const std::string SOURCE = lowercase(config.source);
            return SOURCE == "none" || SOURCE == "legacy";
        }

        std::vector<std::string> readConfiguredThemeNames(const SConfig& config) {
            std::vector<std::string> themes;

            appendUnique(themes, config.theme);
            if (!themes.empty())
                return themes;

            if (themeLookupDisabled(config))
                return themes;

            const std::string SOURCE = lowercase(config.source);
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

        const std::vector<std::string>& configuredThemeNames(SResolverState& state,
                                                             const SConfig&  config) {
            const auto KEY = std::pair{config.theme, config.source};
            if (const auto IT = state.themeNames.find(KEY); IT != state.themeNames.end())
                return IT->second;
            return state.themeNames.emplace(KEY, readConfiguredThemeNames(config)).first->second;
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

        std::optional<fs::path> findIconByTheme(SResolverState& state, const std::string& iconName,
                                                int sizePx, const std::vector<std::string>& themes,
                                                const std::stop_token& stop) {
            const auto LOOKUP_NAME = iconLookupName(iconName);
            for (const auto& themeName : themes) {
                if (stop.stop_requested())
                    return std::nullopt;
                std::unordered_set<std::string> visited;
                if (auto path = findIconInTheme(state, LOOKUP_NAME, themeName, sizePx, visited))
                    return path;
            }

            std::unordered_set<std::string> visited;
            return findIconInTheme(state, LOOKUP_NAME, "hicolor", sizePx, visited);
        }

        const std::unordered_map<std::string, std::vector<fs::path>>*
        legacyIconIndex(SResolverState& state, const std::stop_token& stop) {
            if (state.legacyIconIndex)
                return &*state.legacyIconIndex;

            std::unordered_map<std::string, std::vector<fs::path>> index;
            for (const auto& root : iconRoots()) {
                std::error_code ec;
                for (fs::recursive_directory_iterator
                         it(root, fs::directory_options::skip_permission_denied, ec),
                     end;
                     it != end; it.increment(ec)) {
                    if (stop.stop_requested())
                        return nullptr;
                    if (ec || !hasImageExtension(it->path()) || !it->is_regular_file(ec))
                        continue;
                    index[it->path().stem().string()].push_back(it->path());
                }
            }
            state.legacyIconIndex = std::move(index);
            return &*state.legacyIconIndex;
        }

        std::optional<std::string> findIconByLegacySearch(SResolverState&    state,
                                                          const std::string& iconName, int sizePx,
                                                          const std::stop_token& stop) {
            const auto* index = legacyIconIndex(state, stop);
            if (!index)
                return std::nullopt;
            const auto MATCH = index->find(iconLookupName(iconName));
            if (MATCH == index->end())
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

        std::optional<std::string> findIconPath(SResolverState& state, const std::string& iconName,
                                                int sizePx, const SConfig& config,
                                                const std::stop_token& stop) {
            if (iconName.empty())
                return std::nullopt;

            auto&       cache     = state.iconPathCache;
            const auto& THEMES    = configuredThemeNames(state, config);
            const auto  CACHE_KEY = iconName + ":" + std::to_string(sizePx) + ":" + config.theme +
                ":" + config.source + ":" + themeCacheKeyPart(THEMES);
            if (const auto IT = cache.find(CACHE_KEY); IT != cache.end())
                return IT->second;

            const fs::path  ICON(iconName);
            std::error_code ec;
            if (ICON.is_absolute() && fs::is_regular_file(ICON, ec)) {
                cache.emplace(CACHE_KEY, ICON.string());
                return ICON.string();
            }

            const auto THEME_RESULT = themeLookupDisabled(config) ?
                std::optional<fs::path>{} :
                findIconByTheme(state, iconName, sizePx, THEMES, stop);
            auto       RESULT = THEME_RESULT ? std::optional<std::string>{THEME_RESULT->string()} :
                                               findIconByLegacySearch(state, iconName, sizePx, stop);
            if (stop.stop_requested())
                return std::nullopt;
            cache.emplace(CACHE_KEY, RESULT);
            return RESULT;
        }

        std::optional<std::string> iconOverrideForAppId(const std::string& appId,
                                                        const SConfig&     config) {
            const auto KEY = lowercase(appId);
            for (const auto& entry : splitList(config.overrides, ',')) {
                const auto EQUAL = entry.find('=');
                if (EQUAL == std::string::npos)
                    continue;

                if (Winview::trimmedLower(entry.substr(0, EQUAL)) == KEY) {
                    auto ICON = trim(entry.substr(EQUAL + 1));
                    if (!ICON.empty())
                        return ICON;
                }
            }

            return std::nullopt;
        }

    }

    struct CResolver::SState : SResolverState {};

    CResolver::CResolver() : m_state(std::make_unique<SState>()) {}
    CResolver::~CResolver() = default;

    std::optional<std::string> CResolver::find(const std::vector<std::string>& appIds, int sizePx,
                                               const SConfig& config, const std::stop_token& stop) {
        auto& state = *m_state;
        for (const auto& appId : appIds) {
            if (auto overrideIcon = iconOverrideForAppId(appId, config)) {
                if (auto path = findIconPath(state, *overrideIcon, sizePx, config, stop))
                    return path;
            }

            if (auto entry = findDesktopEntry(state, appId, stop)) {
                if (auto path = findIconPath(state, entry->icon, sizePx, config, stop))
                    return path;
            }

            if (auto path = findIconPath(state, appId, sizePx, config, stop))
                return path;
            if (stop.stop_requested())
                return std::nullopt;
        }

        return std::nullopt;
    }

    bool CResolver::refreshThemes() {
        bool changed = false;
        for (auto& [key, names] : m_state->themeNames) {
            auto current = readConfiguredThemeNames(
                {.theme = key.first, .source = key.second, .overrides = ""});
            if (current != names) {
                names   = std::move(current);
                changed = true;
            }
        }
        if (changed)
            m_state->iconPathCache.clear();
        return changed;
    }

    void CResolver::clear() {
        *m_state = {};
    }
}
