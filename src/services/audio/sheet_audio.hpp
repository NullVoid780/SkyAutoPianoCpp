#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "services/audio/note_synth.hpp"
#include "services/audio/wave_out.hpp"

namespace sky {

class SheetAudio {
public:
    SheetAudio();
    ~SheetAudio();
    SheetAudio(const SheetAudio &) = delete;
    SheetAudio &operator=(const SheetAudio &) = delete;

    bool play(std::vector<SynthNote> notes, double fromMs);
    void stop();
    void preview(int key);
    bool isPlaying() const;
    double positionMs() const;
    bool idle() const;
    void release();
    void setMuted(bool muted) { m_muted = muted; }
    void setSamples(std::shared_ptr<const SampleBank> samples);
    bool hasSamples() const;
    int sampleRate() const { return m_synth.sampleRate(); }
    bool muted() const { return m_muted; }

private:
    bool ensureDevice();
    void renderBlock(float *out, int frames);

    mutable std::mutex m_mutex;
    NoteSynth m_synth;
    WaveOut m_out;
    std::int64_t m_deviceOrigin = 0;
    std::atomic<bool> m_muted{false};
};

}  // namespace sky
