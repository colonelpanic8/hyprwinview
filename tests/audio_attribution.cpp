#include "audio/attribution.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using Audio::eAmbiguous;
using Audio::eDisambiguator;
using Audio::eIndicator;
using Audio::eOwnerVia;
using Audio::SRules;
using Audio::SSnapshot;

using Indicators = std::unordered_map<uint64_t, eIndicator>;

static void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

static constexpr Audio::Pid CHROME_BROWSER = 16787;
static constexpr Audio::Pid CHROME_AUDIO   = 23468;
static constexpr Audio::Pid PLAYERCTLD     = 1463730;
static const std::string    PLAYING_TITLE =
    "MS NOW EXCLUSIVE: Abdul El-Sayed SPEAKS OUT in first interview since Haley Stevens' shock "
    "betrayal";

static SSnapshot chromeSnapshot() {
    SSnapshot snapshot;
    snapshot.parentOf = [](Audio::Pid pid) -> std::optional<Audio::Pid> {
        if (pid == CHROME_AUDIO)
            return CHROME_BROWSER;
        if (pid == CHROME_BROWSER)
            return 9144;
        return 1;
    };
    snapshot.streams.push_back({.id        = 91,
                                .pid       = CHROME_AUDIO,
                                .appName   = "Google Chrome",
                                .binary    = "chrome",
                                .appId     = "",
                                .mediaName = "Playback",
                                .x11Xid    = std::nullopt});

    const std::vector<std::string> TITLES = {
        "colonelpanic8 (Ivan Malison) - Google Chrome",
        "Home / X - Google Chrome",
        "Home / X - Google Chrome",
        "lichess.org • Free Online Chess - Google Chrome",
        "Michigan Senate Election Winner | Polymarket - Google Chrome",
        "Portfolio | Polymarket - Google Chrome",
        "(4) " + PLAYING_TITLE + " - Google Chrome",
        "Portfolio | Polymarket - Google Chrome",
        "Pushbullet - Your devices working better together - Google Chrome",
        "Senate Win Odds - Google Chrome",
        "YouTube - Google Chrome",
        "Polymarket US | Trade on sports, politics, and more | Polymarket - Google Chrome",
        "lichess.org • Free Online Chess - Google Chrome",
        "Portfolio | Polymarket - Google Chrome",
    };
    for (std::size_t i = 0; i < TITLES.size(); ++i)
        snapshot.windows.push_back({.key          = i + 1,
                                    .pid          = CHROME_BROWSER,
                                    .cls          = "google-chrome",
                                    .initialClass = "google-chrome",
                                    .title        = TITLES[i],
                                    .x11Xid       = std::nullopt});
    snapshot.windows.push_back({.key          = 100,
                                .pid          = 36297,
                                .cls          = "spotify",
                                .initialClass = "spotify",
                                .title        = "Spotify Premium",
                                .x11Xid       = std::nullopt});
    return snapshot;
}

static Indicators allChromeWindows(eIndicator indicator) {
    Indicators expected;
    for (uint64_t key = 1; key <= 14; ++key)
        expected[key] = indicator;
    return expected;
}

static void testChromeForegroundTab() {
    auto snapshot = chromeSnapshot();
    snapshot.sessions.push_back({.pid = CHROME_BROWSER, .title = PLAYING_TITLE});

    const auto ATTRIBUTIONS = Audio::attribute(snapshot, {});
    expect(ATTRIBUTIONS.size() == 1, "one audible stream gives one attribution");
    const auto& A = ATTRIBUTIONS[0];
    expect(A.streamId == 91 && A.ownerVia == eOwnerVia::Process, "chrome owner is the browser");
    expect(A.candidates.size() == 14, "all chrome windows are candidates");
    expect(A.narrowedBy == eDisambiguator::MediaSession, "media session narrowed chrome");
    expect(A.windows == std::vector<uint64_t>{7} && A.indicator == eIndicator::Window,
           "chrome audio not tied to the playing tab's window");
    expect(Audio::indicatorsByWindow(ATTRIBUTIONS) == Indicators{{7, eIndicator::Window}},
           "only the playing window is badged");
}

static void testChromeBackgroundTab() {
    auto snapshot = chromeSnapshot();
    snapshot.sessions.push_back({.pid = CHROME_BROWSER, .title = "Lofi hip hop radio"});

    const auto ATTRIBUTIONS = Audio::attribute(snapshot, {});
    expect(!ATTRIBUTIONS[0].narrowedBy, "a session matching no title narrowed");
    expect(Audio::indicatorsByWindow(ATTRIBUTIONS) == allChromeWindows(eIndicator::App),
           "background tab audio did not mark every chrome window as app");
}

static void testAmbiguousNone() {
    auto snapshot = chromeSnapshot();
    snapshot.sessions.push_back({.pid = CHROME_BROWSER, .title = "Lofi hip hop radio"});
    SRules rules;
    rules.apps["google-chrome"] = {.ambiguous = eAmbiguous::None};

    const auto ATTRIBUTIONS = Audio::attribute(snapshot, rules);
    expect(ATTRIBUTIONS[0].ruleApp == "google-chrome", "chrome app rule not chosen");
    expect(ATTRIBUTIONS[0].indicator == eIndicator::None && ATTRIBUTIONS[0].windows.empty(),
           "ambiguous none still badged");
    expect(Audio::indicatorsByWindow(ATTRIBUTIONS).empty(), "ambiguous none left a badge");
}

static void testPlayerctldSessionIgnored() {
    auto snapshot = chromeSnapshot();
    snapshot.sessions.push_back({.pid = PLAYERCTLD, .title = PLAYING_TITLE});

    const auto ATTRIBUTIONS = Audio::attribute(snapshot, {});
    expect(!ATTRIBUTIONS[0].narrowedBy, "a session outside the lineage narrowed");
    expect(Audio::indicatorsByWindow(ATTRIBUTIONS) == allChromeWindows(eIndicator::App),
           "playerctld session changed chrome's badges");
}

static void testShortNeedleIgnored() {
    auto snapshot             = chromeSnapshot();
    snapshot.windows[4].title = "Radio - Google Chrome";
    snapshot.sessions.push_back({.pid = CHROME_BROWSER, .title = "  Radio "});
    expect(Audio::indicatorsByWindow(Audio::attribute(snapshot, {})) ==
               allChromeWindows(eIndicator::App),
           "a five character needle narrowed");

    snapshot.windows[4].title  = "Videos - Google Chrome";
    snapshot.sessions[0].title = "VIDEOS";
    expect(Audio::indicatorsByWindow(Audio::attribute(snapshot, {})) ==
               Indicators{{5, eIndicator::Window}},
           "a six character needle did not narrow case-insensitively");
}

static void testSingleWindowApp() {
    SSnapshot snapshot = chromeSnapshot();
    snapshot.streams   = {{.id        = 68,
                           .pid       = 36297,
                           .appName   = "spotify",
                           .binary    = ".spotify-wrapped",
                           .appId     = "",
                           .mediaName = "Spotify",
                           .x11Xid    = std::nullopt}};

    const auto ATTRIBUTIONS = Audio::attribute(snapshot, {});
    expect(ATTRIBUTIONS[0].ownerVia == eOwnerVia::Process && !ATTRIBUTIONS[0].narrowedBy,
           "spotify not owned by its process");
    expect(Audio::indicatorsByWindow(ATTRIBUTIONS) == Indicators{{100, eIndicator::Window}},
           "spotify window not badged");
}

static void testX11WindowOwner() {
    SSnapshot snapshot;
    snapshot.windows = {
        {.key          = 1,
         .pid          = 500,
         .cls          = "steam",
         .initialClass = "steam",
         .title        = "Steam",
         .x11Xid       = 0x1a00003},
        {.key          = 2,
         .pid          = 500,
         .cls          = "steam",
         .initialClass = "steam",
         .title        = "Friends",
         .x11Xid       = 0x1a00009},
    };
    snapshot.streams = {{.id        = 3,
                         .pid       = 500,
                         .appName   = "Steam",
                         .binary    = "steam",
                         .appId     = "",
                         .mediaName = "",
                         .x11Xid    = 0x1a00009}};

    const auto ATTRIBUTIONS = Audio::attribute(snapshot, {});
    expect(ATTRIBUTIONS[0].ownerVia == eOwnerVia::X11Window, "xid did not take precedence");
    expect(Audio::indicatorsByWindow(ATTRIBUTIONS) == Indicators{{2, eIndicator::Window}},
           "xid owner not badged as window");
}

static void testAppIdFallback() {
    SSnapshot snapshot          = chromeSnapshot();
    snapshot.streams            = {{.id        = 68,
                                    .pid       = 0,
                                    .appName   = "Spotify",
                                    .binary    = ".spotify-wrapped",
                                    .appId     = "",
                                    .mediaName = "Spotify",
                                    .x11Xid    = std::nullopt}};
    snapshot.windows.back().cls = "Spotify";

    const auto ATTRIBUTIONS = Audio::attribute(snapshot, {});
    expect(ATTRIBUTIONS[0].ownerVia == eOwnerVia::AppId, "unknown pid did not fall back to app id");
    expect(Audio::indicatorsByWindow(ATTRIBUTIONS) == Indicators{{100, eIndicator::Window}},
           "app id owner not badged");

    snapshot.streams[0].appName = "mpd";
    snapshot.streams[0].binary  = "mpd";
    expect(Audio::attribute(snapshot, {})[0].ownerVia == std::nullopt,
           "a stream with no owner was attributed");
}

static void testIgnore() {
    SSnapshot snapshot = chromeSnapshot();
    snapshot.streams   = {{.id        = 68,
                           .pid       = 36297,
                           .appName   = "spotify",
                           .binary    = ".spotify-wrapped",
                           .appId     = "",
                           .mediaName = "Spotify",
                           .x11Xid    = std::nullopt}};
    SRules rules;
    rules.apps["spotify"] = {.ignore = true};

    const auto ATTRIBUTIONS = Audio::attribute(snapshot, rules);
    expect(ATTRIBUTIONS[0].ruleApp == "spotify" && ATTRIBUTIONS[0].windows.empty() &&
               ATTRIBUTIONS[0].indicator == eIndicator::None,
           "ignored app was badged");
}

static void testRuleMergePrecedence() {
    auto snapshot = chromeSnapshot();
    snapshot.sessions.push_back({.pid = CHROME_BROWSER, .title = PLAYING_TITLE});

    SRules rules;
    rules.fallback = {.disambiguate = std::vector<eDisambiguator>{}, .ambiguous = eAmbiguous::None};
    expect(Audio::indicatorsByWindow(Audio::attribute(snapshot, rules)).empty(),
           "user default did not override the hardcoded default");

    rules.apps["google-chrome"] = {.ambiguous = eAmbiguous::App};
    expect(Audio::indicatorsByWindow(Audio::attribute(snapshot, rules)) ==
               allChromeWindows(eIndicator::App),
           "app rule did not override ambiguous while keeping the default's disambiguate");

    rules.apps["google-chrome"].disambiguate = std::vector{eDisambiguator::MediaSession};
    expect(Audio::indicatorsByWindow(Audio::attribute(snapshot, rules)) ==
               Indicators{{7, eIndicator::Window}},
           "app rule disambiguate did not override the default");

    rules.fallback.ignore = true;
    expect(Audio::indicatorsByWindow(Audio::attribute(snapshot, rules)).empty(),
           "unset app ignore did not fall through to the user default");
}

// A PWA shares the browser PID, so it joins the browser's candidates (a documented hole).
static void testPwaMixedClasses() {
    auto snapshot = chromeSnapshot();
    snapshot.windows.resize(3);
    snapshot.windows.push_back({.key          = 50,
                                .pid          = CHROME_BROWSER,
                                .cls          = "chrome-cinhimbnkkaebkkfgbkmqihbgnehkgkn-Default",
                                .initialClass = "chrome-cinhimbnkkaebkkfgbkmqihbgnehkgkn-Default",
                                .title        = "YouTube Music",
                                .x11Xid       = std::nullopt});
    snapshot.sessions.push_back({.pid = CHROME_BROWSER, .title = "Never Gonna Give You Up"});

    expect(Audio::indicatorsByWindow(Audio::attribute(snapshot, {})) ==
               Indicators{{1, eIndicator::App},
                          {2, eIndicator::App},
                          {3, eIndicator::App},
                          {50, eIndicator::App}},
           "PWA audio did not mark the browser and PWA windows as app");

    SRules rules;
    rules.apps["google-chrome"]                                   = {.ambiguous = eAmbiguous::None};
    rules.apps["chrome-cinhimbnkkaebkkfgbkmqihbgnehkgkn-default"] = {.ambiguous = eAmbiguous::App};
    const auto ATTRIBUTIONS = Audio::attribute(snapshot, rules);
    expect(ATTRIBUTIONS[0].ruleApp == "google-chrome" && ATTRIBUTIONS[0].windows.empty(),
           "the most frequent candidate class did not choose the rule");

    snapshot.windows[3].title = "Never Gonna Give You Up - YouTube Music";
    expect(Audio::indicatorsByWindow(Audio::attribute(snapshot, {})) ==
               Indicators{{50, eIndicator::Window}},
           "PWA title match did not narrow to the PWA window");
}

static void testStrongestIndicatorWins() {
    const std::vector<Audio::SAttribution> ATTRIBUTIONS = {
        {.streamId = 1, .windows = {1, 2}, .indicator = eIndicator::App},
        {.streamId = 2, .windows = {2}, .indicator = eIndicator::Window},
    };
    expect(Audio::indicatorsByWindow(ATTRIBUTIONS) ==
               Indicators{{1, eIndicator::App}, {2, eIndicator::Window}},
           "a weaker indicator replaced a stronger one");
}

int main() {
    testChromeForegroundTab();
    testChromeBackgroundTab();
    testAmbiguousNone();
    testPlayerctldSessionIgnored();
    testShortNeedleIgnored();
    testSingleWindowApp();
    testX11WindowOwner();
    testAppIdFallback();
    testIgnore();
    testRuleMergePrecedence();
    testPwaMixedClasses();
    testStrongestIndicatorWins();
    std::cout << "Audio attribution tests passed\n";
}
