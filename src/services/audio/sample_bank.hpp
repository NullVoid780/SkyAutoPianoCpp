#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "core/result.hpp"

namespace sky {

struct SampleBank {
    static constexpr int kKeyCount = 15;

    std::string name;
    int sampleRate = 0;
    std::array<std::vector<float>, kKeyCount> keys;

    bool has(int key) const { return key >= 0 && key < kKeyCount && !keys[static_cast<std::size_t>(key)].empty(); }
    int loadedKeys() const;
};

std::vector<float> decodeAudio(const std::vector<unsigned char> &bytes, const std::string &extension, int targetRate);
std::vector<float> decodeAudioFile(const std::filesystem::path &file, int targetRate);
std::vector<float> resampleMono(const std::vector<float> &samples, int fromRate, int toRate);
Result<SampleBank> loadSampleBank(const std::filesystem::path &folder, int targetRate);
Result<SampleBank> loadBuiltInSoundpack(const std::string &instrument, int targetRate);

}  // namespace sky
