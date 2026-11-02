#include "pipewire_streams.hpp"

#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

namespace Audio {
    namespace {
        // Hyprland sets SA_NOCLDWAIT, so the kernel may reap the child first and waitpid then
        // reports ECHILD; either way the child is gone.
        bool reaped(pid_t pid, int options) {
            const auto RESULT = waitpid(pid, nullptr, options);
            return RESULT == pid || (RESULT < 0 && errno != EINTR);
        }
    }

    CPipeWireStreams::CPipeWireStreams(Logger logger) : log(std::move(logger)) {}

    CPipeWireStreams::~CPipeWireStreams() {
        if (output >= 0)
            close(output);
        stopChild();
    }

    int CPipeWireStreams::fd() const {
        return output;
    }

    short CPipeWireStreams::events() const {
        return POLLIN;
    }

    std::optional<Clock::time_point> CPipeWireStreams::deadline() const {
        if (output >= 0)
            return std::nullopt;
        return retryAt;
    }

    bool CPipeWireStreams::failed() const {
        return broken;
    }

    uint64_t CPipeWireStreams::generation() const {
        return changes;
    }

    const std::vector<SStream>& CPipeWireStreams::streams() const {
        return reader.audibleStreams();
    }

    void CPipeWireStreams::spawn() {
        std::array<int, 2> pipeFds{-1, -1};
        // Only the compositor's end is nonblocking. A nonblocking stdout makes pw-dump drop
        // output mid-batch when a burst outruns the pipe.
        if (pipe2(pipeFds.data(), O_CLOEXEC) != 0 || fcntl(pipeFds[0], F_SETFL, O_NONBLOCK) != 0) {
            log(std::format("audio: pipe for pw-dump failed: {}", std::strerror(errno)));
            for (const int FD : pipeFds)
                if (FD >= 0)
                    close(FD);
            retryAt = Clock::now() + RETRY_DELAY;
            return;
        }

        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDOUT_FILENO);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        // The compositor holds many fds without O_CLOEXEC; none belong in the child.
        posix_spawn_file_actions_addclosefrom_np(&actions, STDERR_FILENO + 1);

        posix_spawnattr_t attributes;
        posix_spawnattr_init(&attributes);
        sigset_t signals;
        sigemptyset(&signals);
        posix_spawnattr_setsigmask(&attributes, &signals);
        sigfillset(&signals);
        posix_spawnattr_setsigdefault(&attributes, &signals);
        posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);

        std::array<char*, 4> argv{const_cast<char*>("pw-dump"), const_cast<char*>("-m"),
                                  const_cast<char*>("-N"), nullptr};
        pid_t                pid = -1;
        const int            ERROR =
            posix_spawnp(&pid, "pw-dump", &actions, &attributes, argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        posix_spawnattr_destroy(&attributes);
        close(pipeFds[1]);

        if (ERROR != 0) {
            close(pipeFds[0]);
            log(std::format("audio: cannot run pw-dump: {}", std::strerror(ERROR)));
            retryAt = Clock::now() + RETRY_DELAY;
            return;
        }

        child  = pid;
        output = pipeFds[0];
    }

    void CPipeWireStreams::dispatch() {
        if (broken)
            return;
        if (output < 0) {
            if (Clock::now() >= retryAt)
                spawn();
            return;
        }

        std::array<char, 65536> buffer{};
        while (true) {
            const auto READ = read(output, buffer.data(), buffer.size());
            if (READ < 0 && errno == EINTR)
                continue;
            if (READ < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                return;
            if (READ <= 0) {
                log(READ == 0 ?
                        std::string("audio: pw-dump exited") :
                        std::format("audio: reading pw-dump failed: {}", std::strerror(errno)));
                broken = true;
                return;
            }

            const auto RESULT = reader.feed({buffer.data(), static_cast<std::size_t>(READ)});
            if (!RESULT) {
                log("audio: " + RESULT.error());
                broken = true;
                return;
            }
            if (*RESULT) {
                ++changes;
                log.clear();
            }
        }
    }

    void CPipeWireStreams::reset() {
        if (output >= 0)
            close(output);
        output = -1;
        stopChild();
        if (!reader.audibleStreams().empty())
            ++changes;
        reader.clear();
        broken  = false;
        retryAt = Clock::now() + RETRY_DELAY;
    }

    void CPipeWireStreams::stopChild() {
        if (child <= 0)
            return;
        const pid_t PID = std::exchange(child, -1);
        if (reaped(PID, WNOHANG))
            return;

        // pw-dump holds no state worth flushing, and SIGKILL keeps the wait below short.
        kill(PID, SIGKILL);
        while (!reaped(PID, 0)) {}
    }
}
