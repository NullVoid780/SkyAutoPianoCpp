#include "codec/midi_sheet_parser.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

#include "core/translator.hpp"
#include "codec/midi_file.hpp"
#include "codec/sheet_error.hpp"
#include "core/string_utils.hpp"
#include "model/key_table.hpp"

namespace sky {

namespace {

constexpr int kMetaCopyright = 0x02;
constexpr int kMetaTrackName = 0x03;
constexpr int kMetaTempo = 0x51;
constexpr int kNoteOn = 0x9;
constexpr double kDefaultTempo = 500000.0;
constexpr double kDefaultTimeDivision = 480.0;

constexpr std::array<std::pair<int, int>, kSkyKeyCount> kMidiToSky = {{
    {60, 0}, {62, 1}, {64, 2}, {65, 3}, {67, 4}, {69, 5}, {71, 6}, {72, 7},
    {74, 8}, {76, 9}, {77, 10}, {79, 11}, {81, 12}, {83, 13}, {84, 14},
}};

int skyKeyForMidiNote(double midiNote)
{
    for (const auto &[candidate, index] : kMidiToSky) {
        if (candidate == midiNote) {
            return index;
        }
    }
    double closestDistance = std::numeric_limits<double>::infinity();
    int keyIndex = 0;
    for (const auto &[candidate, index] : kMidiToSky) {
        const double distance = std::fabs(candidate - midiNote);
        if (distance < closestDistance) {
            closestDistance = distance;
            keyIndex = index;
        }
    }
    return keyIndex;
}

double jsRound(double value)
{
    return std::floor(value + 0.5);
}

}  // namespace

Result<OrderedJson> parseMidiSheet(std::span<const std::byte> content, std::string_view fileStem)
{
    if (content.empty()) {
        return sheetError(SheetErrorCode::EmptyMidi, trUtf8("The selected MIDI file is empty."));
    }
    auto parsed = readMidiFile(content);
    if (!parsed) {
        if (parsed.error().code == kMidiNotMidiCode) {
            return sheetError(SheetErrorCode::InvalidMidi, trUtf8("The MIDI file does not contain any tracks."));
        }
        const std::string &detail = parsed.error().details;
        return sheetError(SheetErrorCode::InvalidMidi,
                          fromQString(tr("Invalid MIDI file: %1").arg(toQString(detail))), detail);
    }
    const MidiFile &midi = parsed.value();

    double tempo = kDefaultTempo;
    std::string author = "Unknown";
    std::string transcribedBy = "Unknown";
    std::string name(fileStem);

    for (const MidiTrack &track : midi.tracks) {
        for (const MidiEvent &event : track.events) {
            if (event.type != kMidiMetaEvent) {
                continue;
            }
            if (event.metaType == kMetaTempo && event.value != 0.0 && !std::isnan(event.value)) {
                tempo = event.value;
            }
            if (event.metaType == kMetaCopyright && event.hasText) {
                const std::size_t comma = event.text.find(',');
                author = event.text.substr(0, comma);
                if (comma != std::string::npos) {
                    const std::size_t next = event.text.find(',', comma + 1);
                    transcribedBy = event.text.substr(comma + 1, next == std::string::npos ? std::string::npos : next - comma - 1);
                }
            }
            if (event.metaType == kMetaTrackName && event.hasText) {
                name = event.text;
            }
        }
    }

    const double timeDivision =
        (midi.smpteDivision || midi.timeDivision == 0.0) ? kDefaultTimeDivision : midi.timeDivision;

    OrderedJson songNotes = OrderedJson::array();
    for (const MidiTrack &track : midi.tracks) {
        double absoluteTimeMs = 0.0;
        for (const MidiEvent &event : track.events) {
            absoluteTimeMs += event.deltaTime * (tempo / 1000.0) / timeDivision;
            if (event.type != kNoteOn || event.bytes.size() < 2 || event.bytes[1] <= 0) {
                continue;
            }
            OrderedJson note = OrderedJson::object();
            note["time"] = jsonNumber(jsRound(absoluteTimeMs));
            note["key"] = songKeyName(skyKeyForMidiNote(event.bytes[0]));
            songNotes.push_back(std::move(note));
        }
    }

    const std::string cleanAuthor(trimJs(author));
    const std::string cleanTranscriber(trimJs(transcribedBy));

    OrderedJson sheet = OrderedJson::object();
    sheet["name"] = name;
    sheet["author"] = cleanAuthor.empty() ? "Unknown" : cleanAuthor;
    sheet["transcribedBy"] = cleanTranscriber.empty() ? "Unknown" : cleanTranscriber;
    sheet["bpm"] = jsonNumber(jsRound(60000000.0 / tempo));
    sheet["bitsPerPage"] = 16;
    sheet["pitchLevel"] = 0;
    sheet["isEncrypted"] = false;
    sheet["songNotes"] = std::move(songNotes);
    return sheet;
}

}  // namespace sky
