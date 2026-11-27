#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <memory>
#include <vector>

#include "qt_test_support.hpp"
#include "services/audio/note_synth.hpp"
#include "services/audio/sample_bank.hpp"

using namespace sky;

namespace {

void writeWav(const std::filesystem::path &file, int rate, int channels, const std::vector<std::int16_t> &samples)
{
    std::ofstream out(file, std::ios::binary);
    const auto put32 = [&out](std::uint32_t value) { out.write(reinterpret_cast<const char *>(&value), 4); };
    const auto put16 = [&out](std::uint16_t value) { out.write(reinterpret_cast<const char *>(&value), 2); };
    const auto bytes = static_cast<std::uint32_t>(samples.size() * 2);
    out.write("RIFF", 4);
    put32(36 + bytes);
    out.write("WAVEfmt ", 8);
    put32(16);
    put16(1);
    put16(static_cast<std::uint16_t>(channels));
    put32(static_cast<std::uint32_t>(rate));
    put32(static_cast<std::uint32_t>(rate * channels * 2));
    put16(static_cast<std::uint16_t>(channels * 2));
    put16(16);
    out.write("data", 4);
    put32(bytes);
    out.write(reinterpret_cast<const char *>(samples.data()), bytes);
}

}  // namespace

TEST_CASE("resampling keeps the shape and changes the length")
{
    const std::vector<float> ramp = {0.0f, 1.0f, 2.0f, 3.0f};
    const std::vector<float> doubled = resampleMono(ramp, 22050, 44100);
    REQUIRE(doubled.size() == 7);
    CHECK(doubled[1] == Catch::Approx(0.5));
    CHECK(doubled[6] == Catch::Approx(3.0));
    CHECK(resampleMono(ramp, 44100, 44100) == ramp);
}

TEST_CASE("a stereo wav file is decoded to mono at the player's rate")
{
    TempDir dir;
    const auto file = dir.path() / "0.wav";
    writeWav(file, 22050, 2, {16384, 0, 16384, 0, -16384, 0, -16384, 0});
    const std::vector<float> mono = decodeAudioFile(file, 44100);
    REQUIRE(mono.size() == 7);
    CHECK(mono.front() == Catch::Approx(0.25).margin(0.001));
    CHECK(mono.back() == Catch::Approx(-0.25).margin(0.001));
    CHECK(decodeAudioFile(dir.path() / "missing.wav", 44100).empty());
}

TEST_CASE("a sound folder loads the keys it has and reports what is missing")
{
    TempDir dir;
    CHECK_FALSE(loadSampleBank(dir.path() / "nowhere", 44100));
    CHECK(loadSampleBank(dir.path(), 44100).error().code == "SOUND_FOLDER_EMPTY");
    writeWav(dir.path() / "0.wav", 44100, 1, {1000, 2000});
    writeWav(dir.path() / "3.wav", 44100, 1, {3000});
    writeWav(dir.path() / "99.wav", 44100, 1, {3000});
    auto bank = loadSampleBank(dir.path(), 44100);
    REQUIRE(bank);
    CHECK(bank.value().loadedKeys() == 2);
    CHECK(bank.value().has(0));
    CHECK(bank.value().has(3));
    CHECK_FALSE(bank.value().has(1));
    CHECK(bank.value().keys[3].front() == Catch::Approx(0.9).margin(0.001));
    CHECK(bank.value().keys[0].back() == Catch::Approx(0.6).margin(0.001));
}

TEST_CASE("the synth plays loaded samples and falls back to its own tone")
{
    auto bank = std::make_shared<SampleBank>();
    bank->sampleRate = 44100;
    bank->keys[2] = std::vector<float>(20000, 0.5f);
    NoteSynth synth(44100);
    synth.setSamples(bank);
    REQUIRE(synth.hasSamples());
    synth.trigger(2);
    std::vector<float> out(10);
    synth.render(out.data(), 10);
    CHECK(out[5] == Catch::Approx(std::tanh(0.5 * 0.7)).margin(0.001));
    std::vector<float> rest(20000);
    synth.render(rest.data(), 20000);
    CHECK(synth.voiceCount() == 0);

    synth.trigger(9);
    synth.render(out.data(), 10);
    CHECK(synth.voiceCount() == 1);

    NoteSynth other(48000);
    other.setSamples(bank);
    CHECK_FALSE(other.hasSamples());
}

TEST_CASE("the built-in Sky piano soundpack has every key at the right pitch")
{
    auto bank = loadBuiltInSoundpack("Piano", 44100);
    REQUIRE(bank);
    CHECK(bank.value().loadedKeys() == SampleBank::kKeyCount);
    const auto pitch = [](const std::vector<float> &samples) {
        const std::size_t start = 2205;
        const std::size_t window = 8820;
        double best = 0;
        int bestLag = 1;
        for (int lag = 36; lag < 220; ++lag) {
            double sum = 0;
            for (std::size_t i = start; i < start + window; ++i) {
                sum += samples[i] * samples[i + static_cast<std::size_t>(lag)];
            }
            if (sum > best) {
                best = sum;
                bestLag = lag;
            }
        }
        return 44100.0 / bestLag;
    };
    CHECK(pitch(bank.value().keys[0]) == Catch::Approx(NoteSynth::frequencyOf(0)).epsilon(0.02));
    CHECK(pitch(bank.value().keys[7]) == Catch::Approx(NoteSynth::frequencyOf(7)).epsilon(0.02));
    CHECK(pitch(bank.value().keys[14]) == Catch::Approx(NoteSynth::frequencyOf(14)).epsilon(0.02));
    CHECK_FALSE(loadBuiltInSoundpack("Nothing", 44100));
}
