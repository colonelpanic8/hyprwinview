#include "app_icon_loader.hpp"

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace AppIconLoader;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

static void expect(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

class CFakeSource : public ISource {
  public:
    using LoadFunction = std::function<Icon(const SRequest&, const std::stop_token&)>;
    explicit CFakeSource(LoadFunction load) : m_load(std::move(load)) {}

    Icon load(const SRequest& request, const std::stop_token& stop) override {
        return m_load(request, stop);
    }

    void reset() override {
        ++resets;
    }

    bool refresh() override {
        return themeChanged.exchange(false);
    }

    std::atomic<int>  resets       = 0;
    std::atomic<bool> themeChanged = false;

  private:
    LoadFunction m_load;
};

struct SFakeLoader {
    explicit SFakeLoader(CFakeSource::LoadFunction load, SOptions options = {}) :
        source(new CFakeSource(std::move(load))),
        loader(std::unique_ptr<ISource>(source), options) {}

    CFakeSource* source;
    CLoader      loader;
};

static Icon awaitIcon(CLoader& loader, const SRequest& request) {
    const auto DEADLINE = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < DEADLINE) {
        if (auto result = loader.request(request))
            return *result;
        std::this_thread::sleep_for(1ms);
    }
    throw std::runtime_error("icon load timed out");
}

static Icon awaitChange(CLoader& loader, const SRequest& request, const Icon& previous) {
    const auto DEADLINE = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < DEADLINE) {
        if (auto result = loader.request(request); result && *result != previous)
            return *result;
        std::this_thread::sleep_for(1ms);
    }
    throw std::runtime_error("icon was never replaced");
}

static Icon namedIcon(const std::string& name, int sizePx) {
    return std::make_shared<SLoadedIcon>(name, sizePx, nullptr);
}

static void testQueue() {
    std::promise<void> entered;
    std::promise<void> release;
    const auto         RELEASE = release.get_future().share();
    std::atomic<int>   calls   = 0;
    SFakeLoader        fake([&](const SRequest& request, const std::stop_token&) {
        if (++calls == 1) {
            entered.set_value();
            RELEASE.wait_for(5s);
        }
        return namedIcon(request.config.theme, request.sizePx);
    });
    auto&              loader = fake.loader;
    const SRequest     REQUEST{{"app"}, 48, {"first", "auto", ""}};
    expect(!loader.request(REQUEST), "cold request should be pending");
    expect(entered.get_future().wait_for(5s) == std::future_status::ready, "worker did not start");
    const auto START = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i)
        expect(!loader.request(REQUEST), "unfinished load reported ready");
    const auto DURATION = std::chrono::steady_clock::now() - START;
    expect(DURATION < 1s, "requests blocked on the loader");
    expect(calls == 1, "pending requests were not deduplicated");
    auto changed         = REQUEST;
    changed.config.theme = "second";
    expect(!loader.request(changed), "changed theme reused an old request");
    release.set_value();
    const auto FIRST  = awaitIcon(loader, REQUEST);
    const auto SECOND = awaitIcon(loader, changed);
    expect(FIRST->path == "first" && SECOND->path == "second", "results crossed configurations");
    changed.sizePx = 96;
    expect(awaitIcon(loader, changed)->sizePx == 96, "monitor scale reused a smaller icon");
    expect(loader.takeReady() == FIRST, "completion queue lost first icon");
    expect(loader.takeReady() == SECOND, "completion queue lost second icon");
    expect(loader.takeReady() != nullptr && !loader.takeReady(), "completion queue repeated work");
    std::cout << "1000 requests while worker blocked: "
              << std::chrono::duration<double, std::milli>(DURATION).count() << " ms\n";
}

static void testMissesAndShutdown() {
    std::atomic<int> calls = 0;
    {
        SFakeLoader    fake([&](const SRequest&, const std::stop_token&) -> Icon {
            ++calls;
            throw std::runtime_error("decode failure");
        });
        const SRequest REQUEST{{"missing"}, 48, {"", "auto", ""}};
        expect(!awaitIcon(fake.loader, REQUEST), "failed load should be a completed miss");
        std::this_thread::sleep_for(20ms);
        for (int i = 0; i < 100; ++i)
            expect(fake.loader.request(REQUEST).has_value(), "negative cache lost a miss");
        expect(calls == 1, "failed loads retried each frame");
    }
    calls = 0;
    std::promise<void> entered;
    {
        SFakeLoader fake([&](const SRequest&, const std::stop_token& stop) -> Icon {
            ++calls;
            entered.set_value();
            while (!stop.stop_requested())
                std::this_thread::sleep_for(1ms);
            return nullptr;
        });
        fake.loader.request({{"first"}, 48, {"", "auto", ""}});
        expect(entered.get_future().wait_for(5s) == std::future_status::ready,
               "worker did not start");
        for (int size = 1; size <= 100; ++size)
            fake.loader.request({{"queued"}, size, {"", "auto", ""}});
    }
    expect(calls == 1, "shutdown processed queued work");
}

static void testInvalidateDiscardsStaleCompletion() {
    std::promise<void> entered;
    std::promise<void> release;
    const auto         RELEASE = release.get_future().share();
    std::atomic<int>   calls   = 0;
    SFakeLoader        fake([&](const SRequest& request, const std::stop_token&) {
        const int CALL = ++calls;
        if (CALL == 1) {
            entered.set_value();
            RELEASE.wait_for(5s);
        }
        return namedIcon(std::to_string(CALL), request.sizePx);
    });
    auto&              loader = fake.loader;
    const SRequest     REQUEST{{"app"}, 48, {"", "auto", ""}};
    loader.request(REQUEST);
    expect(entered.get_future().wait_for(5s) == std::future_status::ready, "worker did not start");
    loader.invalidate();
    release.set_value();
    const auto ICON = awaitIcon(loader, REQUEST);
    expect(ICON->path == "2", "a result resolved before invalidation was published");
    expect(fake.source->resets == 1, "invalidation did not reset the source once");
    expect(loader.takeReady() == ICON && !loader.takeReady(), "stale result reached the uploader");

    loader.invalidate();
    expect(loader.request(REQUEST).value() == ICON, "invalidation hid the previous icon");
    expect(awaitChange(loader, REQUEST, ICON)->path == "3", "invalidation did not reload");
    expect(fake.source->resets == 2 && calls == 3, "invalidation queued duplicate work");
}

static void testThemeRefresh() {
    std::atomic<int> calls = 0;
    SFakeLoader      fake(
        [&](const SRequest& request, const std::stop_token&) {
            return namedIcon(std::to_string(++calls), request.sizePx);
        },
        {.pollInterval = 5ms, .maxEntries = 256});
    const SRequest REQUEST{{"app"}, 48, {"", "auto", ""}};
    const auto     FIRST = awaitIcon(fake.loader, REQUEST);
    std::this_thread::sleep_for(50ms);
    expect(calls == 1, "idle polling reloaded unchanged icons");
    fake.source->themeChanged = true;
    expect(awaitChange(fake.loader, REQUEST, FIRST)->path == "2", "theme change was not reloaded");
    expect(fake.source->resets == 0, "theme change discarded unrelated caches");
}

static void testEntriesAreBounded() {
    std::atomic<int> calls = 0;
    SFakeLoader      fake(
        [&](const SRequest& request, const std::stop_token&) {
            ++calls;
            return namedIcon(request.appIds.front(), request.sizePx);
        },
        {.pollInterval = 1s, .maxEntries = 2});
    auto&          loader = fake.loader;
    const SRequest A{{"a"}, 48, {"", "auto", ""}};
    const SRequest B{{"b"}, 48, {"", "auto", ""}};
    const SRequest C{{"c"}, 48, {"", "auto", ""}};
    awaitIcon(loader, A);
    awaitIcon(loader, B);
    loader.request(A);
    awaitIcon(loader, C);
    expect(loader.request(A).has_value(), "recently used entry was evicted");
    expect(!loader.request(B).has_value(), "least recently used entry was kept");
    awaitIcon(loader, B);
    expect(calls == 4, "eviction changed the number of loads");
}

static void testZeroEntryLimitKeepsRequests() {
    SFakeLoader    fake([](const SRequest& request,
                        const std::stop_token&) { return namedIcon("zero", request.sizePx); },
                     {.pollInterval = 1s, .maxEntries = 0});
    const SRequest REQUEST{{"app"}, 48, {"", "auto", ""}};
    expect(!fake.loader.request(REQUEST), "cold request should be pending");
    expect(awaitIcon(fake.loader, REQUEST)->path == "zero", "zero entry limit dropped requests");
}

static void writeSvg(const fs::path& path) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << "<svg xmlns='http://www.w3.org/2000/svg' width='48' height='48'>"
                           "<rect width='48' height='48' fill='red'/></svg>";
}

static void useDataRoot(const fs::path& root) {
    setenv("HOME", (root / "home").c_str(), 1);
    setenv("XDG_DATA_HOME", (root / "data").c_str(), 1);
    setenv("XDG_DATA_DIRS", (root / "system").c_str(), 1);
    setenv("XDG_CONFIG_HOME", (root / "config").c_str(), 1);
    setenv("XDG_CURRENT_DESKTOP", "", 1);
    setenv("QT_QPA_PLATFORMTHEME", "", 1);
}

static void testFileSourceInvalidation(const fs::path& root) {
    useDataRoot(root);
    CLoader        loader;
    const SRequest REQUEST{{"late"}, 48, {"", "legacy", ""}};
    expect(!awaitIcon(loader, REQUEST), "missing icon resolved");
    writeSvg(root / "data/pixmaps/late.svg");
    std::this_thread::sleep_for(20ms);
    expect(!loader.request(REQUEST).value(), "miss was not cached");
    loader.invalidate();
    const auto ICON = awaitChange(loader, REQUEST, nullptr);
    expect(ICON->path == root / "data/pixmaps/late.svg", "invalidation did not find the new icon");
}

static void benchmarkShutdownDuringScan(const fs::path& root) {
    useDataRoot(root);
    for (int dir = 0; dir < 400; ++dir) {
        const auto DIR = root / "system/icons/bulk" / std::to_string(dir);
        fs::create_directories(DIR);
        for (int file = 0; file < 250; ++file)
            std::ofstream(DIR / (std::to_string(file) + ".png"));
    }
    const SRequest REQUEST{{"absent"}, 48, {"", "legacy", ""}};

    const auto     SCAN_START = std::chrono::steady_clock::now();
    expect(!AppIconLookup::CResolver{}.find(REQUEST.appIds, REQUEST.sizePx, REQUEST.config),
           "absent icon resolved");
    const auto SCAN = std::chrono::steady_clock::now() - SCAN_START;

    class CSignalSource : public ISource {
      public:
        explicit CSignalSource(std::promise<void>& entered) : m_entered(entered) {}
        Icon load(const SRequest& request, const std::stop_token& stop) override {
            m_entered.set_value();
            return m_files->load(request, stop);
        }

      private:
        std::promise<void>&      m_entered;
        std::unique_ptr<ISource> m_files = makeFileSource();
    };

    std::promise<void> entered;
    auto               loader = std::make_unique<CLoader>(std::make_unique<CSignalSource>(entered));
    loader->request(REQUEST);
    expect(entered.get_future().wait_for(5s) == std::future_status::ready, "scan did not start");
    std::this_thread::sleep_for(SCAN / 4);
    const auto STOP_START = std::chrono::steady_clock::now();
    loader.reset();
    const auto STOP = std::chrono::steady_clock::now() - STOP_START;

    const auto MS = [](auto duration) {
        return std::chrono::duration<double, std::milli>(duration).count();
    };
    std::cout << "uncancelled scan: " << MS(SCAN) << " ms, shutdown mid-scan: " << MS(STOP)
              << " ms\n";
}

static void testDecode(const fs::path& root) {
    fs::create_directories(root);
    const auto SVG = root / "icon.svg";
    const auto PNG = root / "icon.png";
    const auto BAD = root / "broken.png";
    writeSvg(SVG);
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 48, 48);
    auto* cr      = cairo_create(surface);
    cairo_set_source_rgb(cr, 0, 1, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    expect(cairo_surface_write_to_png(surface, PNG.c_str()) == CAIRO_STATUS_SUCCESS,
           "PNG fixture failed");
    cairo_surface_destroy(surface);
    std::ofstream(BAD) << "invalid png";

    CLoader    loader;
    SRequest   request{{"app"}, 48, {"", "auto", "app=" + SVG.string()}};
    const auto ICON = awaitIcon(loader, request);
    expect(ICON && cairo_image_surface_get_width(ICON->surface.get()) == 48, "SVG decode failed");
    const auto* pixels =
        reinterpret_cast<const uint32_t*>(cairo_image_surface_get_data(ICON->surface.get()));
    expect(pixels[0] == 0xFFFF0000U, "SVG pixels are incorrect");
    auto alias             = request;
    alias.appIds           = {"alias"};
    alias.config.overrides = "alias=" + SVG.string();
    expect(awaitIcon(loader, alias) == ICON, "same image decoded twice for different applications");
    request.config.overrides = "app=" + PNG.string();
    expect(awaitIcon(loader, request)->path == PNG, "PNG decode failed");
    request.config.overrides = "app=" + BAD.string();
    expect(!awaitIcon(loader, request), "corrupt image was accepted");
}

int main(int argc, char** argv) {
    const bool BENCHMARK = argc == 3 && std::string_view(argv[2]) == "--benchmark-shutdown";
    if (argc != 2 && !BENCHMARK)
        return 2;
    const fs::path ROOT = fs::absolute(argv[1]);
    if (BENCHMARK) {
        benchmarkShutdownDuringScan(ROOT / "scan");
        return 0;
    }
    testQueue();
    testMissesAndShutdown();
    testInvalidateDiscardsStaleCompletion();
    testThemeRefresh();
    testEntriesAreBounded();
    testZeroEntryLimitKeepsRequests();
    testDecode(ROOT / "decode");
    testFileSourceInvalidation(ROOT / "invalidate");
    std::cout << "Icon loader tests passed\n";
}
