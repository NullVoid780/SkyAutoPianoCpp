#include "services/audio/sample_bank.hpp"

#include <QFile>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>

#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>
#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>
#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>

#include "core/translator.hpp"
#include "core/string_utils.hpp"

namespace sky {

namespace {

constexpr std::array<const char *, 3> kExtensions = {".mp3", ".wav", ".flac"};
constexpr float kTargetPeak = 0.9f;

std::vector<unsigned char> readBytes(const std::filesystem::path &file)
{
    std::ifstream stream(file, std::ios::binary);
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::vector<float> downmix(const float *frames, std::uint64_t count, unsigned channels)
{
    std::vector<float> mono(static_cast<std::size_t>(count));
    for (std::uint64_t frame = 0; frame < count; ++frame) {
        float sum = 0.0f;
        for (unsigned channel = 0; channel < channels; ++channel) {
            sum += frames[frame * channels + channel];
        }
        mono[static_cast<std::size_t>(frame)] = sum / static_cast<float>(std::max(1u, channels));
    }
    return mono;
}

}  // namespace

int SampleBank::loadedKeys() const
{
    return static_cast<int>(std::count_if(keys.begin(), keys.end(), [](const auto &key) { return !key.empty(); }));
}

std::vector<float> resampleMono(const std::vector<float> &samples, int fromRate, int toRate)
{
    if (samples.empty() || fromRate <= 0 || toRate <= 0 || fromRate == toRate) {
        return samples;
    }
    const double ratio = static_cast<double>(fromRate) / toRate;
    const auto length = static_cast<std::size_t>(std::floor((samples.size() - 1) / ratio)) + 1;
    std::vector<float> out(length);
    for (std::size_t i = 0; i < length; ++i) {
        const double source = i * ratio;
        const auto index = static_cast<std::size_t>(source);
        const double fraction = source - static_cast<double>(index);
        const float next = index + 1 < samples.size() ? samples[index + 1] : samples[index];
        out[i] = static_cast<float>(samples[index] + (next - samples[index]) * fraction);
    }
    return out;
}

std::vector<float> decodeAudioFile(const std::filesystem::path &file, int targetRate)
{
    return decodeAudio(readBytes(file), toLowerAscii(fromWide(file.extension().wstring())), targetRate);
}

std::vector<float> decodeAudio(const std::vector<unsigned char> &bytes, const std::string &extension, int targetRate)
{
    if (bytes.empty()) {
        return {};
    }
    unsigned channels = 0;
    unsigned rate = 0;
    std::uint64_t frames = 0;
    float *data = nullptr;
    if (extension == ".mp3") {
        drmp3_config config{};
        drmp3_uint64 count = 0;
        data = drmp3_open_memory_and_read_pcm_frames_f32(bytes.data(), bytes.size(), &config, &count, nullptr);
        channels = config.channels;
        rate = config.sampleRate;
        frames = count;
    } else if (extension == ".wav") {
        drwav_uint64 count = 0;
        data = drwav_open_memory_and_read_pcm_frames_f32(bytes.data(), bytes.size(), &channels, &rate, &count, nullptr);
        frames = count;
    } else if (extension == ".flac") {
        drflac_uint64 count = 0;
        data = drflac_open_memory_and_read_pcm_frames_f32(bytes.data(), bytes.size(), &channels, &rate, &count, nullptr);
        frames = count;
    }
    if (!data) {
        return {};
    }
    std::vector<float> mono = downmix(data, frames, channels);
    if (extension == ".mp3") {
        drmp3_free(data, nullptr);
    } else if (extension == ".wav") {
        drwav_free(data, nullptr);
    } else {
        drflac_free(data, nullptr);
    }
    return resampleMono(mono, static_cast<int>(rate), targetRate);
}

namespace {

void normalize(SampleBank &bank)
{
    float peak = 0.0f;
    for (const std::vector<float> &key : bank.keys) {
        for (float sample : key) {
            peak = std::max(peak, std::fabs(sample));
        }
    }
    if (peak > 0.0f) {
        const float gain = kTargetPeak / peak;
        for (std::vector<float> &key : bank.keys) {
            for (float &sample : key) {
                sample *= gain;
            }
        }
    }
}

}  // namespace

Result<SampleBank> loadSampleBank(const std::filesystem::path &folder, int targetRate)
{
    SampleBank bank;
    bank.name = fromWide(folder.filename().wstring());
    bank.sampleRate = targetRate;
    std::error_code error;
    if (!std::filesystem::is_directory(folder, error)) {
        return Error{"SOUND_FOLDER_MISSING", trUtf8("The soundpack folder was not found."), fromWide(folder.wstring())};
    }
    for (int key = 0; key < SampleBank::kKeyCount; ++key) {
        for (const char *extension : kExtensions) {
            const std::filesystem::path file = folder / (std::to_string(key) + extension);
            if (std::filesystem::is_regular_file(file, error)) {
                bank.keys[static_cast<std::size_t>(key)] = decodeAudioFile(file, targetRate);
                break;
            }
        }
    }
    if (bank.loadedKeys() == 0) {
        return Error{"SOUND_FOLDER_EMPTY", trUtf8("The soundpack folder has no notes in it."), fromWide(folder.wstring())};
    }
    normalize(bank);
    return bank;
}

Result<SampleBank> loadBuiltInSoundpack(const std::string &instrument, int targetRate)
{
    SampleBank bank;
    bank.name = instrument;
    bank.sampleRate = targetRate;
    for (int key = 0; key < SampleBank::kKeyCount; ++key) {
        for (const char *extension : kExtensions) {
            QFile file(QStringLiteral(":/soundpack/%1/%2%3")
                           .arg(QString::fromStdString(instrument))
                           .arg(key)
                           .arg(QLatin1String(extension)));
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QByteArray data = file.readAll();
            const std::vector<unsigned char> bytes(data.begin(), data.end());
            bank.keys[static_cast<std::size_t>(key)] = decodeAudio(bytes, extension, targetRate);
            break;
        }
    }
    if (bank.loadedKeys() == 0) {
        return Error{"SOUNDPACK_MISSING", trUtf8("The built-in soundpack was not found."), instrument};
    }
    normalize(bank);
    return bank;
}

}  // namespace sky
