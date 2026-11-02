#include "audio/pw_dump.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

using Audio::SStream;

static void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

// Trimmed from captured `pw-dump -m -N` batches for a Spotify (pipewire-pulse) stream and a
// pw-play (native) stream. Spotify was paused in the capture; its state is set to running here.
static constexpr std::string_view INITIAL = R"([
  {
    "id": 66,
    "type": "PipeWire:Interface:Client",
    "version": 3,
    "permissions": [ "r", "w", "x", "m" ],
    "info": {
      "change-mask": [ "props" ],
      "props": {
        "application.name": "spotify",
        "application.process.binary": ".spotify-wrapped",
        "application.process.id": 36297,
        "client.api": "pipewire-pulse",
        "pipewire.sec.pid": 1383337,
        "window.x11.display": ":0"
      }
    }
  },
  {
    "id": 68,
    "type": "PipeWire:Interface:Node",
    "version": 3,
    "permissions": [ "r", "w", "x", "m" ],
    "info": {
      "change-mask": [ "input-ports", "output-ports", "state", "props", "params" ],
      "state": "running",
      "error": null,
      "props": {
        "application.name": "spotify",
        "application.process.binary": ".spotify-wrapped",
        "application.process.id": 36297,
        "client.api": "pipewire-pulse",
        "client.id": 66,
        "media.class": "Stream/Output/Audio",
        "media.name": "Spotify",
        "object.id": 68
      },
      "params": {
        "Props": [
          {
            "volume": 1.000000,
            "mute": false,
            "channelVolumes": [ 0.715755, 0.715755 ],
            "channelMap": [ "FL", "FR" ],
            "softMute": false,
            "softVolumes": [ 1.000000, 1.000000 ]
          }
        ]
      }
    }
  },
  {
    "id": 3,
    "info": null
  },
  {
    "id": 70,
    "type": "PipeWire:Interface:Node",
    "version": 3,
    "permissions": [ "r", "x" ],
    "info": {
      "state": "running",
      "props": {
        "media.class": "Audio/Sink",
        "node.name": "alsa_output.usb"
      }
    }
  }
]
)";

static constexpr std::string_view PW_PLAY_STARTED = R"([
  {
    "id": 71,
    "type": "PipeWire:Interface:Client",
    "version": 3,
    "permissions": [ "r", "w", "x", "m" ],
    "info": {
      "change-mask": [ "props" ],
      "props": {
        "application.name": "pw-cat",
        "application.process.binary": "pw-cat",
        "application.process.id": 294259,
        "pipewire.protocol": "protocol-native",
        "pipewire.sec.pid": 294259
      }
    }
  },
  {
    "id": 100,
    "type": "PipeWire:Interface:Node",
    "version": 3,
    "permissions": [ "r", "w", "x", "m", "l" ],
    "info": {
      "change-mask": [ "state" ],
      "state": "running",
      "error": null,
      "props": {
        "application.name": "pw-play",
        "client.id": 71,
        "media.class": "Stream/Output/Audio",
        "media.name": "hwv-silence-test",
        "object.id": 100
      },
      "params": {
        "Props": [
          {
            "volume": 1.000000,
            "mute": false,
            "channelVolumes": [ 1.000000, 1.000000 ],
            "softMute": false
          }
        ]
      }
    }
  }
]
)";

static constexpr std::string_view PW_PLAY_REMOVED = R"([
  {
    "id": 100,
    "info": null
  }
]
)";

static const SStream              SPOTIFY = {.id        = 68,
                                             .pid       = 36297,
                                             .appName   = "spotify",
                                             .binary    = ".spotify-wrapped",
                                             .appId     = "",
                                             .mediaName = "Spotify",
                                             .x11Xid    = std::nullopt};

static void                       testBatches() {
    Audio::CPwDumpReader reader;

    const auto           SPLIT = INITIAL.find("\"softMute\"");
    expect(reader.feed(INITIAL.substr(0, SPLIT)) == false && reader.audibleStreams().empty(),
                                 "a partial batch was applied");
    expect(reader.feed(INITIAL.substr(SPLIT)) == true, "the completed batch was not applied");
    expect(reader.audibleStreams() == std::vector{SPOTIFY},
                                 "pulse stream PID did not come from application.process.id");

    expect(reader.feed(PW_PLAY_STARTED) == true, "native stream not added");
    const SStream PW_PLAY = {.id        = 100,
                                                   .pid       = 294259,
                                                   .appName   = "pw-play",
                                                   .binary    = "pw-cat",
                                                   .appId     = "",
                                                   .mediaName = "hwv-silence-test",
                                                   .x11Xid    = std::nullopt};
    expect(reader.audibleStreams() == std::vector{SPOTIFY, PW_PLAY},
                                 "native stream did not take its PID and binary from the client object");

    expect(reader.feed(PW_PLAY_REMOVED) == true, "removal record did not change the streams");
    expect(reader.audibleStreams() == std::vector{SPOTIFY}, "removed node is still audible");
    expect(reader.feed(PW_PLAY_REMOVED) == false, "a repeated removal reported a change");
}

static void testSilencedStreams() {
    auto withProps = [](std::string_view from, std::string_view to) {
        std::string batch(INITIAL);
        batch.replace(batch.find(from), from.size(), to);
        Audio::CPwDumpReader reader;
        expect(reader.feed(batch).has_value(), "a modified batch failed to parse");
        return reader.audibleStreams();
    };

    expect(withProps("\"mute\": false", "\"mute\": true").empty(), "muted stream is audible");
    expect(withProps("\"softMute\": false", "\"softMute\": true").empty(),
           "soft-muted stream is audible");
    expect(withProps("[ 0.715755, 0.715755 ]", "[ 0.0, 0.0 ]").empty(),
           "zero-volume stream is audible");
    expect(withProps("\"state\": \"running\"", "\"state\": \"idle\"").empty(),
           "idle stream is audible");
    expect(withProps("[ 0.715755, 0.715755 ]", "[ 0.0, 0.5 ]") == std::vector{SPOTIFY},
           "one audible channel did not count");
}

static void testMalformedBatch() {
    Audio::CPwDumpReader reader;
    expect(!reader.feed("[\n  { \"id\": 1,\n]\n"), "a malformed batch was accepted");
}

int main() {
    testBatches();
    testSilencedStreams();
    testMalformedBatch();
    std::cout << "pw-dump reader tests passed\n";
}
