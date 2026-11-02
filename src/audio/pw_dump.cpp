#include "pw_dump.hpp"

#include <glaze/json.hpp>

#include <algorithm>
#include <charconv>
#include <format>
#include <utility>

namespace Audio {
    namespace {
        using Json = glz::generic;

        constexpr std::size_t MAX_PENDING_BYTES = std::size_t{64} << 20;

        const Json*           field(const Json& object, std::string_view key) {
            if (!object.is_object())
                return nullptr;
            const auto& MAP = object.get_object();
            const auto  IT  = MAP.find(key);
            return IT == MAP.end() ? nullptr : &IT->second;
        }

        std::string string(const Json* value) {
            const auto* text = value ? value->get_if<std::string>() : nullptr;
            return text ? *text : std::string{};
        }

        // Accepts a JSON number or a decimal string; PulseAudio clients send props as strings.
        std::optional<int64_t> integer(const Json* value) {
            if (!value)
                return std::nullopt;
            if (const auto* number = value->get_if<double>())
                return static_cast<int64_t>(*number);
            if (const auto* text = value->get_if<std::string>()) {
                int64_t    parsed = 0;
                const auto RESULT =
                    std::from_chars(text->data(), text->data() + text->size(), parsed);
                if (RESULT.ec == std::errc{} && RESULT.ptr == text->data() + text->size())
                    return parsed;
            }
            return std::nullopt;
        }

        std::optional<Pid> pid(const Json* value) {
            const auto PARSED = integer(value);
            if (!PARSED || *PARSED <= 0)
                return std::nullopt;
            return static_cast<Pid>(*PARSED);
        }

        std::optional<uint32_t> id(const Json* value) {
            const auto PARSED = integer(value);
            if (!PARSED || *PARSED < 0)
                return std::nullopt;
            return static_cast<uint32_t>(*PARSED);
        }

        CPwDumpReader::SProps readProps(const Json& props) {
            auto appId = string(field(props, "application.id"));
            if (appId.empty())
                appId = string(field(props, "pipewire.access.portal.app_id"));
            return {
                .api       = string(field(props, "client.api")),
                .appName   = string(field(props, "application.name")),
                .binary    = string(field(props, "application.process.binary")),
                .appId     = std::move(appId),
                .mediaName = string(field(props, "media.name")),
                .processId = pid(field(props, "application.process.id")),
                .secPid    = pid(field(props, "pipewire.sec.pid")),
                .x11Xid    = id(field(props, "window.x11.xid")),
                .clientId  = id(field(props, "client.id")),
            };
        }

        bool bool_(const Json* value) {
            const auto* flag = value ? value->get_if<bool>() : nullptr;
            return flag && *flag;
        }

        // Running and not silenced by mute or zero volume. Missing volume params count as
        // audible; only explicit evidence silences a stream.
        bool audible(const Json& info) {
            if (string(field(info, "state")) != "running")
                return false;

            const auto* params = field(info, "params");
            const auto* props  = params ? field(*params, "Props") : nullptr;
            if (!props || !props->is_array())
                return true;
            for (const auto& entry : props->get_array()) {
                if (bool_(field(entry, "mute")) || bool_(field(entry, "softMute")))
                    return false;
                const auto* volumes = field(entry, "channelVolumes");
                if (!volumes || !volumes->is_array())
                    continue;
                const auto& VALUES = volumes->get_array();
                if (std::ranges::none_of(VALUES, [](const Json& volume) {
                        const auto* number = volume.get_if<double>();
                        return number && *number > 0.0;
                    }))
                    return false;
            }
            return true;
        }

        template <typename T>
        T firstSet(const T& primary, const T& fallback) {
            if constexpr (std::is_same_v<T, std::string>)
                return primary.empty() ? fallback : primary;
            else
                return primary ? primary : fallback;
        }
    }

    std::expected<bool, std::string> CPwDumpReader::feed(std::string_view bytes) {
        pending.append(bytes);

        bool        applied = false;
        std::size_t start   = 0;
        while (true) {
            const auto NEWLINE = pending.find('\n', start);
            if (NEWLINE == std::string::npos)
                break;
            const std::string_view LINE(pending.data() + start, NEWLINE - start);
            start = NEWLINE + 1;
            if (LINE != "]" && LINE != "]\r")
                continue;

            auto result = applyBatch(pending.substr(0, start));
            pending.erase(0, start);
            start = 0;
            if (!result)
                return std::unexpected(std::move(result.error()));
            applied = true;
        }

        if (pending.size() > MAX_PENDING_BYTES)
            return std::unexpected("pw-dump batch exceeds 64 MiB without a terminator");
        if (!applied)
            return false;
        auto next = computeStreams();
        if (next == streams)
            return false;
        streams = std::move(next);
        return true;
    }

    std::expected<void, std::string> CPwDumpReader::applyBatch(const std::string& json) {
        Json batch;
        if (const auto ERROR = glz::read_json(batch, json))
            return std::unexpected(
                std::format("unreadable pw-dump batch: {}", glz::format_error(ERROR, json)));
        if (!batch.is_array())
            return std::unexpected("pw-dump batch is not an array");

        for (const auto& record : batch.get_array()) {
            const auto ID = id(field(record, "id"));
            if (!ID)
                continue;
            const auto* info = field(record, "info");
            if (!info || info->is_null()) {
                nodes.erase(*ID);
                clients.erase(*ID);
                continue;
            }

            const auto  TYPE  = string(field(record, "type"));
            const auto* props = field(*info, "props");
            if (TYPE == "PipeWire:Interface:Client" && props)
                clients.insert_or_assign(*ID, readProps(*props));
            else if (TYPE == "PipeWire:Interface:Node" && props &&
                     string(field(*props, "media.class")) == "Stream/Output/Audio")
                nodes.insert_or_assign(*ID, SNode{readProps(*props), audible(*info)});
            else if (TYPE == "PipeWire:Interface:Node")
                nodes.erase(*ID);
        }
        return {};
    }

    std::vector<SStream> CPwDumpReader::computeStreams() const {
        std::vector<SStream> result;
        for (const auto& [nodeId, node] : nodes) {
            if (!node.audible)
                continue;

            const SProps* client = nullptr;
            if (node.props.clientId) {
                const auto IT = clients.find(*node.props.clientId);
                client        = IT == clients.end() ? nullptr : &IT->second;
            }
            static const SProps NONE;
            const auto&         C = client ? *client : NONE;
            const auto&         N = node.props;

            // pipewire-pulse clients carry the server's PID in pipewire.sec.pid, so the
            // application's own claim is the better evidence there.
            const auto PROCESS_ID = firstSet(N.processId, C.processId);
            const auto PID        = firstSet(N.api, C.api) == "pipewire-pulse" ?
                       PROCESS_ID :
                       firstSet(firstSet(C.secPid, N.secPid), PROCESS_ID);

            result.push_back({
                .id        = nodeId,
                .pid       = PID.value_or(0),
                .appName   = firstSet(N.appName, C.appName),
                .binary    = firstSet(N.binary, C.binary),
                .appId     = firstSet(N.appId, C.appId),
                .mediaName = N.mediaName,
                .x11Xid    = firstSet(N.x11Xid, C.x11Xid),
            });
        }
        std::ranges::sort(result, {}, &SStream::id);
        return result;
    }

    const std::vector<SStream>& CPwDumpReader::audibleStreams() const {
        return streams;
    }

    void CPwDumpReader::clear() {
        pending.clear();
        nodes.clear();
        clients.clear();
        streams.clear();
    }
}
