#ifndef HYPRWINVIEW_AUDIO_SOURCE_HPP
#define HYPRWINVIEW_AUDIO_SOURCE_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace Audio {
    using Clock  = std::chrono::steady_clock;
    using Logger = std::function<void(std::string_view)>;

    inline constexpr auto RETRY_DELAY = std::chrono::seconds(5);

    // An evidence adapter driven by an external poll loop: the compositor's wl_event_loop or the
    // CLI's poll(2). After every dispatch() the driver re-reads fd(), events() and deadline().
    // When failed() turns true the driver must stop watching fd() before it calls reset(), which
    // closes that fd and schedules a reconnect through deadline().
    class IEvidenceSource {
      public:
        virtual ~IEvidenceSource() = default;

        // -1 when there is nothing to watch.
        virtual int fd() const = 0;
        // poll(2) event bits for fd().
        virtual short                            events() const   = 0;
        virtual std::optional<Clock::time_point> deadline() const = 0;
        // Runs on fd activity or once the deadline passes.
        virtual void dispatch()     = 0;
        virtual bool failed() const = 0;
        virtual void reset()        = 0;
        // Bumped whenever the evidence changes.
        virtual uint64_t generation() const = 0;
    };

    // Logs a message unless it repeats the previous one, so a reconnect loop logs once.
    class CLogOnce {
      public:
        explicit CLogOnce(Logger sink) : sink(std::move(sink)) {}

        void operator()(std::string message) {
            if (message == last)
                return;
            last = std::move(message);
            if (sink)
                sink(last);
        }

        void clear() {
            last.clear();
        }

      private:
        Logger      sink;
        std::string last;
    };
}

#endif
