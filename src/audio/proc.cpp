#include "proc.hpp"

#include <charconv>
#include <format>
#include <fstream>
#include <iterator>
#include <string>

namespace Audio {
    std::optional<Pid> parentPid(Pid pid) {
        if (pid <= 0)
            return std::nullopt;

        std::ifstream     file(std::format("/proc/{}/stat", pid));
        const std::string STAT{std::istreambuf_iterator<char>(file), {}};

        // "pid (comm) state ppid ...": comm may contain spaces and parentheses.
        const auto CLOSE = STAT.rfind(')');
        if (CLOSE == std::string::npos || CLOSE + 4 >= STAT.size())
            return std::nullopt;
        const auto PPID_START = STAT.find(' ', CLOSE + 2);
        if (PPID_START == std::string::npos)
            return std::nullopt;

        Pid         parent = 0;
        const char* begin  = STAT.data() + PPID_START + 1;
        const auto  RESULT = std::from_chars(begin, STAT.data() + STAT.size(), parent);
        if (RESULT.ec != std::errc{} || parent <= 0)
            return std::nullopt;
        return parent;
    }
}
