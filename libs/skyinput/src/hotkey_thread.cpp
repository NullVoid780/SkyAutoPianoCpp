#include "hotkey_thread.hpp"

#include <windows.h>

namespace skyinput {

namespace {

constexpr UINT kRunTasks = WM_APP + 1;

}  // namespace

HotkeyThread &HotkeyThread::instance()
{
    static HotkeyThread thread;
    return thread;
}

HotkeyThread::HotkeyThread()
{
    std::promise<unsigned long> started;
    std::future<unsigned long> threadId = started.get_future();
    m_thread = std::thread(&HotkeyThread::loop, this, std::move(started));
    m_threadId = threadId.get();
}

HotkeyThread::~HotkeyThread()
{
    PostThreadMessageW(m_threadId, WM_QUIT, 0, 0);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void HotkeyThread::loop(std::promise<unsigned long> started)
{
    MSG message{};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    m_running.store(true);
    started.set_value(GetCurrentThreadId());

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == kRunTasks) {
            drainTasks();
        } else if (message.message == WM_HOTKEY) {
            dispatch(static_cast<int>(message.wParam));
        }
    }
    m_running.store(false);
    for (auto &[id, entry] : m_entries) {
        unregisterEntry(id, entry);
    }
    drainTasks();
}

void HotkeyThread::execute(std::function<void()> task)
{
    if (GetCurrentThreadId() == m_threadId) {
        task();
        return;
    }
    if (!m_running.load()) {
        return;
    }
    std::packaged_task<void()> packaged(std::move(task));
    std::future<void> done = packaged.get_future();
    {
        std::lock_guard lock(m_taskMutex);
        m_tasks.push(std::move(packaged));
    }
    PostThreadMessageW(m_threadId, kRunTasks, 0, 0);
    done.wait();
}

void HotkeyThread::drainTasks()
{
    while (true) {
        std::packaged_task<void()> task;
        {
            std::lock_guard lock(m_taskMutex);
            if (m_tasks.empty()) {
                return;
            }
            task = std::move(m_tasks.front());
            m_tasks.pop();
        }
        task();
    }
}

void HotkeyThread::dispatch(int id)
{
    const auto it = m_entries.find(id);
    if (it == m_entries.end() || !it->second.active) {
        return;
    }
    Entry &entry = it->second;
    const int state = GetKeyState(entry.virtualKey) & 1;
    if (state == entry.previousState) {
        return;
    }
    entry.previousState = state;
    const std::function<void()> callback = entry.callback;
    if (callback) {
        callback();
    }
}

void HotkeyThread::unregisterEntry(int id, Entry &entry)
{
    if (entry.active) {
        UnregisterHotKey(nullptr, id);
        entry.active = false;
    }
}

int HotkeyThread::allocateId()
{
    return m_nextId.fetch_add(1);
}

Result<void> HotkeyThread::add(int id, std::uint16_t virtualKey, unsigned modifiers, std::function<void()> callback)
{
    Result<void> result;
    execute([&] {
        for (auto &[otherId, other] : m_entries) {
            if (otherId != id && other.active && other.virtualKey == virtualKey && other.modifiers == modifiers) {
                unregisterEntry(otherId, other);
            }
        }
        Entry &entry = m_entries[id];
        unregisterEntry(id, entry);
        entry.virtualKey = virtualKey;
        entry.modifiers = modifiers;
        entry.callback = std::move(callback);
        entry.previousState = 2;
        if (RegisterHotKey(nullptr, id, modifiers | MOD_NOREPEAT, virtualKey)) {
            entry.active = true;
        } else {
            result = Error::fromLastError("RegisterHotKey");
        }
    });
    return result;
}

void HotkeyThread::remove(int id)
{
    execute([&] {
        const auto it = m_entries.find(id);
        if (it != m_entries.end()) {
            unregisterEntry(id, it->second);
            m_entries.erase(it);
        }
    });
}

void HotkeyThread::deactivate(int id)
{
    execute([&] {
        const auto it = m_entries.find(id);
        if (it != m_entries.end()) {
            unregisterEntry(id, it->second);
        }
    });
}

void HotkeyThread::deactivateAll()
{
    execute([&] {
        for (auto &[id, entry] : m_entries) {
            unregisterEntry(id, entry);
        }
    });
}

bool HotkeyThread::isActive(int id)
{
    bool active = false;
    execute([&] {
        const auto it = m_entries.find(id);
        active = it != m_entries.end() && it->second.active;
    });
    return active;
}

}  // namespace skyinput
