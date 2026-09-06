#include "app_icon_loader.hpp"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>

using namespace AppIconLoader;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

static void expect(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

static Icon awaitIcon(CLoader& loader, const SRequest& request) {
    const auto DEADLINE = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < DEADLINE) {
        if (auto result = loader.request(request))
            return *result;
        std::this_thread::sleep_for(1ms);
    }
    throw std::runtime_error("icon load timed out");
}

static void testQueue() {
    std::promise<void> entered;
    std::promise<void> release;
    const auto         RELEASE = release.get_future().share();
    std::atomic<int>   calls   = 0;
    CLoader            loader([&](const SRequest& request, const std::stop_token&) {
        if (++calls == 1) {
            entered.set_value();
            RELEASE.wait_for(5s);
        }
        return std::make_shared<SLoadedIcon>(request.config.theme, request.sizePx, nullptr);
    });
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
        CLoader        loader([&](const SRequest&, const std::stop_token&) -> Icon {
            ++calls;
            throw std::runtime_error("decode failure");
        });
        const SRequest REQUEST{{"missing"}, 48, {"", "auto", ""}};
        expect(!awaitIcon(loader, REQUEST), "failed load should be a completed miss");
        for (int i = 0; i < 100; ++i)
            expect(loader.request(REQUEST).has_value(), "negative cache lost a miss");
        expect(calls == 1, "failed loads retried each frame");
    }
    calls = 0;
    std::promise<void> entered;
    {
        CLoader loader([&](const SRequest&, const std::stop_token& stop) -> Icon {
            ++calls;
            entered.set_value();
            while (!stop.stop_requested())
                std::this_thread::sleep_for(1ms);
            return nullptr;
        });
        loader.request({{"first"}, 48, {"", "auto", ""}});
        expect(entered.get_future().wait_for(5s) == std::future_status::ready,
               "worker did not start");
        for (int size = 1; size <= 100; ++size)
            loader.request({{"queued"}, size, {"", "auto", ""}});
    }
    expect(calls == 1, "shutdown processed queued work");
}

static void testRefresh() {
    std::atomic<int>   calls = 0;
    std::promise<void> entered;
    std::promise<void> release;
    const auto         RELEASE = release.get_future().share();
    CLoader            loader([&](const SRequest& request, const std::stop_token&) {
        if (++calls == 2) {
            entered.set_value();
            RELEASE.wait_for(5s);
        }
        return std::make_shared<SLoadedIcon>(std::to_string(calls.load()), request.sizePx, nullptr);
    });
    const SRequest     REQUEST{{"refresh"}, 48, {"", "auto", ""}};
    const auto         FIRST = awaitIcon(loader, REQUEST);
    std::this_thread::sleep_for(1100ms);
    expect(loader.request(REQUEST).value() == FIRST, "refresh discarded the ready icon");
    expect(entered.get_future().wait_for(5s) == std::future_status::ready, "refresh did not start");
    expect(loader.request(REQUEST).value() == FIRST, "pending refresh lost the previous icon");
    release.set_value();
    const auto DEADLINE = std::chrono::steady_clock::now() + 5s;
    while (loader.request(REQUEST).value() == FIRST && std::chrono::steady_clock::now() < DEADLINE)
        std::this_thread::sleep_for(1ms);
    expect(loader.request(REQUEST).value()->path == "2", "refresh did not publish the new result");
    expect(calls == 2, "refresh queued duplicate work");
}

static void testDecode(const fs::path& root) {
    fs::create_directories(root);
    const auto SVG = root / "icon.svg";
    const auto PNG = root / "icon.png";
    const auto BAD = root / "broken.png";
    std::ofstream(SVG) << "<svg xmlns='http://www.w3.org/2000/svg' width='48' height='48'>"
                          "<rect width='48' height='48' fill='red'/></svg>";
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
    if (argc != 2)
        return 2;
    testQueue();
    testMissesAndShutdown();
    testRefresh();
    testDecode(fs::absolute(argv[1]));
    std::cout << "Icon loader tests passed\n";
}
