#include "codec/midi_file.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "core/string_utils.hpp"

namespace sky {

namespace {

constexpr double kHeaderChunk = 0x4D546864;
constexpr double kTrackChunk = 0x4D54726B;

struct OutOfBounds {};

class ByteReader {
public:
    explicit ByteReader(std::span<const std::byte> data) : m_data(data) {}

    void move(std::ptrdiff_t offset) { m_pointer += offset; }

    double readInt(double requested)
    {
        const double remaining = static_cast<double>(m_data.size()) - static_cast<double>(m_pointer);
        const double count = std::min(requested, remaining);
        if (count < 1) {
            return -1;
        }
        const auto n = static_cast<std::ptrdiff_t>(count);
        double value = 0.0;
        for (std::ptrdiff_t i = 1; i <= n - 1; ++i) {
            value += byteAt(m_pointer) * std::pow(256.0, static_cast<double>(n - i));
            ++m_pointer;
        }
        value += byteAt(m_pointer);
        ++m_pointer;
        return value;
    }

    std::string readText(double length)
    {
        std::string out;
        for (double i = 1; i <= length; ++i) {
            const double c = readInt(1);
            if (c < 0) {
                appendUtf8(out, 0xFFFF);
                break;
            }
            appendUtf8(out, static_cast<char32_t>(c));
        }
        return out;
    }

    double readVariableLength()
    {
        if (m_pointer >= static_cast<std::ptrdiff_t>(m_data.size())) {
            return -1;
        }
        if (checkedByte(m_pointer) < 128) {
            return readInt(1);
        }
        std::vector<double> firstBytes;
        while (checkedByte(m_pointer) >= 128) {
            firstBytes.push_back(readInt(1) - 128);
        }
        const double lastByte = readInt(1);
        double value = 0.0;
        for (std::size_t dt = 1; dt <= firstBytes.size(); ++dt) {
            value += firstBytes[firstBytes.size() - dt] * std::pow(128.0, static_cast<double>(dt));
        }
        return value + lastByte;
    }

private:
    double byteAt(std::ptrdiff_t index) const
    {
        return static_cast<double>(static_cast<unsigned char>(m_data[static_cast<std::size_t>(index)]));
    }

    double checkedByte(std::ptrdiff_t index) const
    {
        if (index < 0 || index >= static_cast<std::ptrdiff_t>(m_data.size())) {
            throw OutOfBounds{};
        }
        return byteAt(index);
    }

    std::span<const std::byte> m_data;
    std::ptrdiff_t m_pointer = 0;
};

Error notMidi()
{
    return Error{kMidiNotMidiCode, "The MIDI file does not contain any tracks.", {}};
}

Error malformed(std::string detail)
{
    return Error{"MIDI_MALFORMED", detail, detail};
}

void readMetaEvent(ByteReader &reader, MidiEvent &event, bool &endOfTrack)
{
    event.type = kMidiMetaEvent;
    event.metaType = static_cast<int>(reader.readInt(1));
    const double length = reader.readVariableLength();
    switch (event.metaType) {
    case 0x2F:
    case -1:
        endOfTrack = true;
        break;
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
        event.hasText = true;
        event.text = reader.readText(length);
        break;
    case 0x21:
    case 0x59:
    case 0x51:
        event.value = reader.readInt(length);
        break;
    case 0x54:
        for (int i = 0; i < 5; ++i) {
            event.bytes.push_back(reader.readInt(1));
        }
        break;
    case 0x58:
        for (int i = 0; i < 4; ++i) {
            event.bytes.push_back(reader.readInt(1));
        }
        break;
    default:
        reader.readInt(length);
        event.value = reader.readInt(length);
        break;
    }
}

void readChannelEvent(ByteReader &reader, MidiEvent &event, int status)
{
    event.type = status >> 4;
    event.channel = status & 0x0F;
    switch (event.type) {
    case 0xF:
        event.value = reader.readInt(reader.readVariableLength());
        break;
    case 0xA:
    case 0xB:
    case 0xE:
    case 0x8:
    case 0x9:
        event.bytes.push_back(reader.readInt(1));
        event.bytes.push_back(reader.readInt(1));
        break;
    case 0xC:
    case 0xD:
        event.value = reader.readInt(1);
        break;
    default:
        break;
    }
}

Result<MidiFile> readChunks(ByteReader &reader)
{
    if (reader.readInt(4) != kHeaderChunk) {
        return notMidi();
    }
    reader.readInt(4);
    MidiFile midi;
    midi.formatType = reader.readInt(2);
    midi.declaredTracks = reader.readInt(2);
    const double divisionHigh = reader.readInt(1);
    const double divisionLow = reader.readInt(1);
    if (divisionHigh >= 128) {
        midi.smpteDivision = true;
    } else {
        midi.timeDivision = divisionHigh * 256 + divisionLow;
    }

    for (double t = 1; t <= midi.declaredTracks; ++t) {
        MidiTrack &track = midi.tracks.emplace_back();
        const double chunk = reader.readInt(4);
        if (chunk == -1) {
            break;
        }
        if (chunk != kTrackChunk) {
            return notMidi();
        }
        reader.readInt(4);

        bool endOfTrack = false;
        std::optional<int> lastStatus;
        while (!endOfTrack) {
            MidiEvent &event = track.events.emplace_back();
            event.deltaTime = reader.readVariableLength();
            int status = static_cast<int>(reader.readInt(1));
            if (status == -1) {
                break;
            }
            if (status >= 128) {
                lastStatus = status;
            } else {
                if (!lastStatus) {
                    return malformed("Cannot read properties of undefined (reading 'toString')");
                }
                status = *lastStatus;
                reader.move(-1);
            }
            if (status == kMidiMetaEvent) {
                readMetaEvent(reader, event, endOfTrack);
            } else {
                readChannelEvent(reader, event, status);
            }
        }
    }
    return midi;
}

}  // namespace

Result<MidiFile> readMidiFile(std::span<const std::byte> content)
{
    ByteReader reader(content);
    try {
        return readChunks(reader);
    } catch (const OutOfBounds &) {
        return malformed("Offset is outside the bounds of the DataView");
    }
}

}  // namespace sky
