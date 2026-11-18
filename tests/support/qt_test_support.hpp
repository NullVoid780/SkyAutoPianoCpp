#pragma once

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <random>
#include <string>
#include <vector>

#include "skyinput/sender.hpp"

inline QCoreApplication &ensureCoreApp()
{
    static int argc = 1;
    static char name[] = "skyautopiano_tests";
    static char *argv[] = {name, nullptr};
    static QCoreApplication *app = QCoreApplication::instance() ? nullptr : new QCoreApplication(argc, argv);
    return *QCoreApplication::instance();
}

inline bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 5000)
{
    ensureCoreApp();
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > timeoutMs) {
            return false;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return true;
}

class TempDir {
public:
    TempDir()
    {
        std::random_device device;
        m_path = std::filesystem::temp_directory_path() / ("sky-test-" + std::to_string(device()) + std::to_string(device()));
        std::filesystem::create_directories(m_path);
    }

    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    TempDir(const TempDir &) = delete;
    TempDir &operator=(const TempDir &) = delete;

    const std::filesystem::path &path() const { return m_path; }

private:
    std::filesystem::path m_path;
};

struct RecordedKey {
    std::uint16_t virtualKey = 0;
    bool down = false;
    double atMs = 0.0;
};

class RecordingSender final : public skyinput::ISender {
public:
    RecordingSender() : m_start(std::chrono::steady_clock::now()) {}

    skyinput::Result<void> toggleKey(skyinput::KeyCode key, bool down) override
    {
        const double at = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_start).count();
        std::lock_guard lock(m_mutex);
        m_events.push_back({skyinput::virtualKey(key), down, at});
        return {};
    }
    skyinput::Result<void> printChars(std::wstring_view) override { return {}; }
    skyinput::Result<void> toggleMouse(skyinput::MouseButton, bool) override { return {}; }
    skyinput::Result<void> move(skyinput::Point, bool) override { return {}; }
    skyinput::Result<void> scrollWheel(int) override { return {}; }
    skyinput::Point cursorPosition() const override { return {}; }
    skyinput::Point lastCoords() const override { return {}; }
    void setTarget(skyinput::WindowHandle) override {}
    skyinput::WindowHandle target() const override { return nullptr; }

    std::vector<RecordedKey> events() const
    {
        std::lock_guard lock(m_mutex);
        return m_events;
    }

    int heldCount() const
    {
        std::lock_guard lock(m_mutex);
        int held[256] = {};
        for (const RecordedKey &event : m_events) {
            held[event.virtualKey & 0xFF] = event.down ? 1 : 0;
        }
        int count = 0;
        for (int value : held) {
            count += value;
        }
        return count;
    }

private:
    std::chrono::steady_clock::time_point m_start;
    mutable std::mutex m_mutex;
    std::vector<RecordedKey> m_events;
};
