#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"

namespace sky {

inline constexpr int kMidiMetaEvent = 0xFF;

struct MidiEvent {
    double deltaTime = 0.0;
    int type = -1;
    int metaType = -1;
    int channel = 0;
    std::vector<double> bytes;
    double value = 0.0;
    bool hasText = false;
    std::string text;
};

struct MidiTrack {
    std::vector<MidiEvent> events;
};

struct MidiFile {
    double formatType = 0.0;
    double declaredTracks = 0.0;
    bool smpteDivision = false;
    double timeDivision = 0.0;
    std::vector<MidiTrack> tracks;
};

inline constexpr const char *kMidiNotMidiCode = "MIDI_NOT_RECOGNISED";

Result<MidiFile> readMidiFile(std::span<const std::byte> content);

}  // namespace sky
