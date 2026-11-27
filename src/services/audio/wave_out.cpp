#include "services/audio/wave_out.hpp"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace sky {

namespace {

constexpr DWORD kWaitMs = 50;
constexpr float kFullScale = 32767.0f;

}  // namespace

struct WaveOut::Device {
    HWAVEOUT handle = nullptr;
    HANDLE event = nullptr;
    std::array<WAVEHDR, kBlocks> headers{};
    std::array<std::vector<std::int16_t>, kBlocks> buffers;
    std::vector<float> scratch;
};

WaveOut::WaveOut() = default;

WaveOut::~WaveOut()
{
    stop();
}

bool WaveOut::start(int sampleRate, Render render)
{
    if (m_running) {
        return true;
    }
    auto device = std::make_unique<Device>();
    device->event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!device->event) {
        return false;
    }
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = static_cast<DWORD>(sampleRate);
    format.wBitsPerSample = 16;
    format.nBlockAlign = 2;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    if (waveOutOpen(&device->handle, WAVE_MAPPER, &format, reinterpret_cast<DWORD_PTR>(device->event), 0,
                    CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        CloseHandle(device->event);
        return false;
    }
    device->scratch.assign(kBlockFrames, 0.0f);
    for (std::size_t i = 0; i < device->headers.size(); ++i) {
        device->buffers[i].assign(kBlockFrames, 0);
        WAVEHDR &header = device->headers[i];
        header.lpData = reinterpret_cast<LPSTR>(device->buffers[i].data());
        header.dwBufferLength = static_cast<DWORD>(kBlockFrames * sizeof(std::int16_t));
        waveOutPrepareHeader(device->handle, &header, sizeof(WAVEHDR));
    }
    m_render = std::move(render);
    m_device = std::move(device);
    m_running = true;
    m_thread = std::thread([this] { run(); });
    return true;
}

void WaveOut::run()
{
    Device &device = *m_device;
    const auto fill = [this, &device](std::size_t index) {
        m_render(device.scratch.data(), kBlockFrames);
        std::vector<std::int16_t> &buffer = device.buffers[index];
        for (int i = 0; i < kBlockFrames; ++i) {
            const float value = std::clamp(device.scratch[static_cast<std::size_t>(i)], -1.0f, 1.0f);
            buffer[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(std::lround(value * kFullScale));
        }
        waveOutWrite(device.handle, &device.headers[index], sizeof(WAVEHDR));
    };
    for (std::size_t i = 0; i < device.headers.size(); ++i) {
        fill(i);
    }
    while (m_running) {
        WaitForSingleObject(device.event, kWaitMs);
        for (std::size_t i = 0; i < device.headers.size() && m_running; ++i) {
            if (device.headers[i].dwFlags & WHDR_DONE) {
                fill(i);
            }
        }
    }
}

void WaveOut::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    SetEvent(m_device->event);
    if (m_thread.joinable()) {
        m_thread.join();
    }
    waveOutReset(m_device->handle);
    for (WAVEHDR &header : m_device->headers) {
        waveOutUnprepareHeader(m_device->handle, &header, sizeof(WAVEHDR));
    }
    waveOutClose(m_device->handle);
    CloseHandle(m_device->event);
    m_device.reset();
}

std::int64_t WaveOut::playedFrames() const
{
    if (!m_running || !m_device) {
        return 0;
    }
    MMTIME time{};
    time.wType = TIME_SAMPLES;
    if (waveOutGetPosition(m_device->handle, &time, sizeof(MMTIME)) != MMSYSERR_NOERROR || time.wType != TIME_SAMPLES) {
        return 0;
    }
    return static_cast<std::int64_t>(time.u.sample);
}

}  // namespace sky
