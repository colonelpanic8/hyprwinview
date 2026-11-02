#ifndef HYPRWINVIEW_AUDIO_PROC_HPP
#define HYPRWINVIEW_AUDIO_PROC_HPP

#include <optional>

#include "attribution.hpp"

namespace Audio {
    // Reads the parent PID from /proc/<pid>/stat.
    std::optional<Pid> parentPid(Pid pid);
}

#endif
