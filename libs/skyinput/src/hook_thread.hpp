#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "skyinput/error.hpp"

namespace skyinput {

enum class HookKind { Keyboard, Mouse };
enum class HookGroup { Listener, Blocker };

struct HookEvent {
    HookKind kind = HookKind::Keyboard;
    unsigned message = 0;
    std::int32_t virtualKey = 0;
    std::int16_t mouseData = 0;
    bool injected = false;
};

using HookHandler = std::function<bool(const HookEvent &)>;

class HookThread {
public:
    static HookThread &instance();
    ~HookThread();

    HookThread(const HookThread &) = delete;
    HookThread &operator=(const HookThread &) = delete;

    Result<int> addHandler(HookKind kind, HookGroup group, HookHandler handler);
    void removeHandler(int id);
    void removeGroup(HookGroup group);
    void execute(std::function<void()> task);

    bool dispatch(const HookEvent &event);

private:
    struct Handler {
        int id = 0;
        HookKind kind = HookKind::Keyboard;
        HookGroup group = HookGroup::Listener;
        HookHandler callback;
    };

    HookThread();
    void loop(std::promise<unsigned long> started);
    void drainTasks();
    Result<void> ensureHook(HookKind kind);
    void releaseUnusedHooks();

    std::thread m_thread;
    unsigned long m_threadId = 0;
    std::atomic<bool> m_running{false};
    std::mutex m_taskMutex;
    std::queue<std::packaged_task<void()>> m_tasks;
    std::vector<Handler> m_handlers;
    void *m_keyboardHook = nullptr;
    void *m_mouseHook = nullptr;
    int m_nextId = 1;
};

}  // namespace skyinput
