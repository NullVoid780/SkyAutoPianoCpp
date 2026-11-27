#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

namespace sky {

class WaveOut {
public:
    using Render = std::function<void(float *out, int frames)>;

    static constexpr int kBlocks = 4;
    static constexpr int kBlockFrames = 512;

    WaveOut();
    ~WaveOut();
    WaveOut(const WaveOut &) = delete;
    WaveOut &operator=(const WaveOut &) = delete;

    bool start(int sampleRate, Render render);
    void stop();
    bool isRunning() const { return m_running; }
    std::int64_t playedFrames() const;

private:
    struct Device;

    void run();

    std::unique_ptr<Device> m_device;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    Render m_render;
};

}  // namespace sky
