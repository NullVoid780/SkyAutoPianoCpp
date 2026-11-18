#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <queue>
#include <thread>

#include "skyinput/error.hpp"

namespace skyinput {

class HotkeyThread {
public:
    static HotkeyThread &instance();
    ~HotkeyThread();

    HotkeyThread(const HotkeyThread &) = delete;
    HotkeyThread &operator=(const HotkeyThread &) = delete;

    int allocateId();
    Result<void> add(int id, std::uint16_t virtualKey, unsigned modifiers, std::function<void()> callback);
    void remove(int id);
    void deactivate(int id);
    void deactivateAll();
    bool isActive(int id);

private:
    struct Entry {
        std::uint16_t virtualKey = 0;
        unsigned modifiers = 0;
        std::function<void()> callback;
        int previousState = 2;
        bool active = false;
    };

    HotkeyThread();
    void loop(std::promise<unsigned long> started);
    void execute(std::function<void()> task);
    void drainTasks();
    void dispatch(int id);
    void unregisterEntry(int id, Entry &entry);

    std::thread m_thread;
    unsigned long m_threadId = 0;
    std::atomic<bool> m_running{false};
    std::mutex m_taskMutex;
    std::queue<std::packaged_task<void()>> m_tasks;
    std::map<int, Entry> m_entries;
    std::atomic<int> m_nextId{1};
};

}  // namespace skyinput
