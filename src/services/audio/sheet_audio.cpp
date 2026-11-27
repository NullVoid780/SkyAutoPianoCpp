#include "services/audio/sheet_audio.hpp"

#include <algorithm>

namespace sky {

SheetAudio::SheetAudio() = default;

SheetAudio::~SheetAudio()
{
    release();
}

bool SheetAudio::ensureDevice()
{
    if (m_out.isRunning()) {
        return true;
    }
    {
        const std::lock_guard lock(m_mutex);
        m_deviceOrigin = m_synth.renderedFrames();
    }
    return m_out.start(m_synth.sampleRate(), [this](float *out, int frames) { renderBlock(out, frames); });
}

void SheetAudio::renderBlock(float *out, int frames)
{
    {
        const std::lock_guard lock(m_mutex);
        m_synth.render(out, frames);
    }
    if (m_muted) {
        std::fill(out, out + frames, 0.0f);
    }
}

bool SheetAudio::play(std::vector<SynthNote> notes, double fromMs)
{
    if (!ensureDevice()) {
        return false;
    }
    const std::lock_guard lock(m_mutex);
    m_synth.silence();
    m_synth.schedule(std::move(notes), fromMs);
    return m_synth.scheduleActive();
}

void SheetAudio::stop()
{
    const std::lock_guard lock(m_mutex);
    m_synth.cancelSchedule();
    m_synth.silence();
}

void SheetAudio::preview(int key)
{
    if (m_muted || !ensureDevice()) {
        return;
    }
    const std::lock_guard lock(m_mutex);
    m_synth.trigger(key);
}

bool SheetAudio::isPlaying() const
{
    const std::lock_guard lock(m_mutex);
    return m_synth.scheduleActive();
}

double SheetAudio::positionMs() const
{
    const std::int64_t played = m_deviceOrigin + m_out.playedFrames();
    const std::lock_guard lock(m_mutex);
    const double elapsed =
        static_cast<double>(played - m_synth.scheduleOriginFrame()) * 1000.0 / m_synth.sampleRate();
    return m_synth.scheduleStartMs() + std::max(0.0, elapsed);
}

bool SheetAudio::idle() const
{
    const std::lock_guard lock(m_mutex);
    return !m_synth.scheduleActive() && m_synth.voiceCount() == 0;
}

void SheetAudio::setSamples(std::shared_ptr<const SampleBank> samples)
{
    const std::lock_guard lock(m_mutex);
    m_synth.setSamples(std::move(samples));
}

bool SheetAudio::hasSamples() const
{
    const std::lock_guard lock(m_mutex);
    return m_synth.hasSamples();
}

void SheetAudio::release()
{
    m_out.stop();
}

}  // namespace sky
