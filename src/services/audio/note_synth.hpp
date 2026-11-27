#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "services/audio/sample_bank.hpp"

namespace sky {

struct SynthNote {
    std::int64_t time = 0;
    int key = 0;
    std::int64_t hold = 0;
};

class NoteSynth {
public:
    static constexpr int kDefaultSampleRate = 44100;
    static constexpr int kMaxVoices = 48;
    static constexpr int kPartials = 6;
    static constexpr int kKeyCount = 15;

    explicit NoteSynth(int sampleRate = kDefaultSampleRate);

    static double frequencyOf(int key);

    int sampleRate() const { return m_sampleRate; }
    void setSamples(std::shared_ptr<const SampleBank> samples);
    bool hasSamples() const { return m_samples != nullptr; }
    void schedule(std::vector<SynthNote> notes, double fromMs);
    void cancelSchedule();
    void trigger(int key, std::int64_t holdMs = 0);
    void silence();
    void render(float *out, int frames);

    bool scheduleActive() const { return m_active; }
    double scheduleStartMs() const { return m_startMs; }
    std::int64_t scheduleOriginFrame() const { return m_originFrame; }
    std::int64_t renderedFrames() const { return m_frame; }
    int voiceCount() const;

private:
    struct Voice {
        std::array<double, kPartials> phase{};
        std::array<double, kPartials> step{};
        std::array<double, kPartials> level{};
        std::array<double, kPartials> decay{};
        const std::vector<float> *sample = nullptr;
        std::size_t position = 0;
        std::int64_t age = 0;
        std::int64_t holdFrames = 0;
        bool released = false;
        bool active = false;
    };

    void startVoice(int key, std::int64_t holdMs);
    void releaseVoice(Voice &voice);
    std::int64_t frameOf(std::int64_t timeMs) const;

    int m_sampleRate;
    std::shared_ptr<const SampleBank> m_samples;
    std::array<Voice, kMaxVoices> m_voices{};
    std::vector<SynthNote> m_notes;
    std::size_t m_next = 0;
    std::int64_t m_frame = 0;
    std::int64_t m_originFrame = 0;
    std::int64_t m_endFrame = 0;
    double m_startMs = 0.0;
    bool m_active = false;
};

}  // namespace sky
