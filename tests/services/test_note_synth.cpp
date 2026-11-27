#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "services/audio/note_synth.hpp"

using namespace sky;

namespace {

constexpr int kRate = 44100;

std::vector<float> renderMs(NoteSynth &synth, int ms)
{
    std::vector<float> samples(static_cast<std::size_t>(kRate * ms / 1000));
    synth.render(samples.data(), static_cast<int>(samples.size()));
    return samples;
}

float peak(const std::vector<float> &samples)
{
    float level = 0.0f;
    for (float sample : samples) {
        level = std::max(level, std::fabs(sample));
    }
    return level;
}

}  // namespace

TEST_CASE("the synth plays the Sky keys at C major pitches")
{
    CHECK(NoteSynth::frequencyOf(0) == Catch::Approx(261.63).margin(0.01));
    CHECK(NoteSynth::frequencyOf(3) == Catch::Approx(349.23).margin(0.01));
    CHECK(NoteSynth::frequencyOf(7) == Catch::Approx(523.25).margin(0.01));
    CHECK(NoteSynth::frequencyOf(14) == Catch::Approx(1046.5).margin(0.01));
}

TEST_CASE("a triggered note sounds and then fades away")
{
    NoteSynth synth(kRate);
    CHECK(peak(renderMs(synth, 20)) == 0.0f);
    synth.trigger(7);
    CHECK(synth.voiceCount() == 1);
    CHECK(peak(renderMs(synth, 100)) > 0.05f);
    renderMs(synth, 12000);
    CHECK(synth.voiceCount() == 0);
    CHECK(peak(renderMs(synth, 20)) == 0.0f);
}

TEST_CASE("scheduled notes start on time and the schedule ends after the last one")
{
    NoteSynth synth(kRate);
    synth.schedule({{100, 4, 0}, {300, 9, 0}}, 0);
    REQUIRE(synth.scheduleActive());
    CHECK(peak(renderMs(synth, 90)) == 0.0f);
    CHECK(peak(renderMs(synth, 30)) > 0.02f);
    renderMs(synth, 600);
    CHECK_FALSE(synth.scheduleActive());
}

TEST_CASE("playback from the middle skips the earlier notes")
{
    NoteSynth synth(kRate);
    synth.schedule({{0, 4, 0}, {500, 9, 0}}, 400);
    CHECK(synth.scheduleStartMs() == 400);
    CHECK(peak(renderMs(synth, 90)) == 0.0f);
    CHECK(peak(renderMs(synth, 30)) > 0.02f);

    NoteSynth empty(kRate);
    empty.schedule({{0, 4, 0}}, 100);
    CHECK_FALSE(empty.scheduleActive());
}

TEST_CASE("held notes ring longer than plucked ones")
{
    NoteSynth plucked(kRate);
    NoteSynth held(kRate);
    plucked.trigger(5);
    held.trigger(5, 2000);
    renderMs(plucked, 1500);
    renderMs(held, 1500);
    CHECK(peak(renderMs(held, 50)) > peak(renderMs(plucked, 50)) * 2);
}

TEST_CASE("many notes at once stay in range and share a fixed number of voices")
{
    NoteSynth synth(kRate);
    for (int i = 0; i < 60; ++i) {
        synth.trigger(i % NoteSynth::kKeyCount);
    }
    CHECK(synth.voiceCount() == NoteSynth::kMaxVoices);
    const std::vector<float> samples = renderMs(synth, 200);
    CHECK(peak(samples) <= 1.0f);
    CHECK(std::none_of(samples.begin(), samples.end(), [](float sample) { return std::isnan(sample); }));
}
