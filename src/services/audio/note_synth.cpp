#include "services/audio/note_synth.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sky {

namespace {

constexpr std::array<int, NoteSynth::kKeyCount> kMidiNotes = {60, 62, 64, 65, 67, 69, 71, 72,
                                                              74, 76, 77, 79, 81, 83, 84};
constexpr std::array<double, NoteSynth::kPartials> kPartialLevels = {1.0, 0.42, 0.2, 0.11, 0.06, 0.035};
constexpr double kInharmonicity = 0.0004;
constexpr double kBaseDecaySeconds = 1.6;
constexpr double kDecayPitchCurve = 0.45;
constexpr double kPartialDecaySpread = 0.7;
constexpr double kHeldDecayStretch = 4.0;
constexpr double kReleaseSeconds = 0.15;
constexpr double kAttackSeconds = 0.003;
constexpr double kTailSeconds = 0.4;
constexpr double kNyquistShare = 0.45;
constexpr double kSilentLevel = 1e-4;
constexpr double kGain = 0.22;
constexpr double kSampleGain = 0.7;
constexpr double kSampleFadeSeconds = 0.08;
constexpr double kReferenceHz = 261.63;
constexpr double kTwoPi = 2.0 * std::numbers::pi;

}  // namespace

NoteSynth::NoteSynth(int sampleRate) : m_sampleRate(std::max(8000, sampleRate)) {}

double NoteSynth::frequencyOf(int key)
{
    const int midi = kMidiNotes[static_cast<std::size_t>(std::clamp(key, 0, kKeyCount - 1))];
    return 440.0 * std::pow(2.0, (midi - 69) / 12.0);
}

void NoteSynth::schedule(std::vector<SynthNote> notes, double fromMs)
{
    std::stable_sort(notes.begin(), notes.end(), [](const SynthNote &a, const SynthNote &b) { return a.time < b.time; });
    std::erase_if(notes, [fromMs](const SynthNote &note) { return static_cast<double>(note.time) < fromMs; });
    m_notes = std::move(notes);
    m_next = 0;
    m_startMs = fromMs;
    m_originFrame = m_frame;
    std::int64_t end = static_cast<std::int64_t>(std::ceil(fromMs));
    for (const SynthNote &note : m_notes) {
        end = std::max(end, note.time + std::max<std::int64_t>(0, note.hold));
    }
    m_endFrame = frameOf(end) + static_cast<std::int64_t>(kTailSeconds * m_sampleRate);
    m_active = !m_notes.empty();
}

void NoteSynth::setSamples(std::shared_ptr<const SampleBank> samples)
{
    silence();
    m_samples = samples && samples->sampleRate == m_sampleRate && samples->loadedKeys() > 0 ? std::move(samples)
                                                                                         : nullptr;
}

void NoteSynth::cancelSchedule()
{
    m_active = false;
    m_notes.clear();
    m_next = 0;
}

void NoteSynth::trigger(int key, std::int64_t holdMs)
{
    startVoice(key, holdMs);
}

void NoteSynth::silence()
{
    for (Voice &voice : m_voices) {
        voice.active = false;
    }
}

int NoteSynth::voiceCount() const
{
    return static_cast<int>(std::count_if(m_voices.begin(), m_voices.end(), [](const Voice &voice) { return voice.active; }));
}

std::int64_t NoteSynth::frameOf(std::int64_t timeMs) const
{
    return m_originFrame + std::llround((static_cast<double>(timeMs) - m_startMs) * m_sampleRate / 1000.0);
}

void NoteSynth::startVoice(int key, std::int64_t holdMs)
{
    if (key < 0 || key >= kKeyCount) {
        return;
    }
    auto slot = std::find_if(m_voices.begin(), m_voices.end(), [](const Voice &voice) { return !voice.active; });
    if (slot == m_voices.end()) {
        slot = std::max_element(m_voices.begin(), m_voices.end(),
                                [](const Voice &a, const Voice &b) { return a.age < b.age; });
    }
    Voice &voice = *slot;
    voice.sample = m_samples && m_samples->has(key) ? &m_samples->keys[static_cast<std::size_t>(key)] : nullptr;
    voice.position = 0;
    const double frequency = frequencyOf(key);
    const double baseDecay = kBaseDecaySeconds * std::pow(kReferenceHz / frequency, kDecayPitchCurve);
    const bool held = holdMs > 0;
    for (int partial = 0; partial < kPartials; ++partial) {
        const int harmonic = partial + 1;
        const double hz = frequency * harmonic * (1.0 + kInharmonicity * harmonic * harmonic);
        const auto index = static_cast<std::size_t>(partial);
        const double seconds = baseDecay / (1.0 + kPartialDecaySpread * partial) * (held ? kHeldDecayStretch : 1.0);
        voice.phase[index] = 0.0;
        voice.step[index] = kTwoPi * hz / m_sampleRate;
        voice.level[index] = hz < m_sampleRate * kNyquistShare ? kPartialLevels[index] : 0.0;
        voice.decay[index] = std::exp(-1.0 / (seconds * m_sampleRate));
    }
    voice.age = 0;
    voice.holdFrames = held ? holdMs * m_sampleRate / 1000 : 0;
    voice.released = false;
    voice.active = true;
}

void NoteSynth::releaseVoice(Voice &voice)
{
    if (voice.sample) {
        voice.released = true;
        return;
    }
    const double decay = std::exp(-1.0 / (kReleaseSeconds * m_sampleRate));
    voice.decay.fill(decay);
    voice.released = true;
}

void NoteSynth::render(float *out, int frames)
{
    const double attackFrames = kAttackSeconds * m_sampleRate;
    for (int i = 0; i < frames; ++i) {
        while (m_active && m_next < m_notes.size() && frameOf(m_notes[m_next].time) <= m_frame) {
            startVoice(m_notes[m_next].key, m_notes[m_next].hold);
            ++m_next;
        }
        double sample = 0.0;
        for (Voice &voice : m_voices) {
            if (!voice.active) {
                continue;
            }
            if (voice.holdFrames > 0 && !voice.released && voice.age >= voice.holdFrames) {
                releaseVoice(voice);
            }
            if (voice.sample) {
                const std::vector<float> &data = *voice.sample;
                const std::size_t remaining = data.size() - voice.position;
                const double fade = std::min(1.0, remaining / (kSampleFadeSeconds * m_sampleRate));
                sample += data[voice.position] * fade * kSampleGain / kGain;
                ++voice.position;
                ++voice.age;
                if (voice.position >= data.size()) {
                    voice.active = false;
                }
                continue;
            }
            double value = 0.0;
            double total = 0.0;
            for (std::size_t partial = 0; partial < static_cast<std::size_t>(kPartials); ++partial) {
                value += voice.level[partial] * std::sin(voice.phase[partial]);
                total += voice.level[partial];
                voice.level[partial] *= voice.decay[partial];
                voice.phase[partial] += voice.step[partial];
                if (voice.phase[partial] >= kTwoPi) {
                    voice.phase[partial] -= kTwoPi;
                }
            }
            sample += value * std::min(1.0, voice.age / attackFrames);
            ++voice.age;
            if (total < kSilentLevel) {
                voice.active = false;
            }
        }
        out[i] = static_cast<float>(std::tanh(sample * kGain));
        ++m_frame;
        if (m_active && m_next >= m_notes.size() && m_frame >= m_endFrame) {
            m_active = false;
        }
    }
}

}  // namespace sky
