#include "hook_thread.hpp"

#include <windows.h>

#include <algorithm>

namespace skyinput {

namespace {

constexpr UINT kRunTasks = WM_APP + 1;

LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0) {
        const auto *data = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);
        HookEvent event;
        event.kind = HookKind::Keyboard;
        event.message = static_cast<unsigned>(wParam);
        event.virtualKey = static_cast<std::int32_t>(data->vkCode);
        event.injected = (data->flags & LLKHF_INJECTED) != 0;
        if (HookThread::instance().dispatch(event)) {
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK mouseProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0) {
        const auto *data = reinterpret_cast<const MSLLHOOKSTRUCT *>(lParam);
        HookEvent event;
        event.kind = HookKind::Mouse;
        event.message = static_cast<unsigned>(wParam);
        event.mouseData = static_cast<std::int16_t>(HIWORD(data->mouseData));
        event.virtualKey = event.mouseData;
        event.injected = (data->flags & LLMHF_INJECTED) != 0;
        if (HookThread::instance().dispatch(event)) {
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

}  // namespace

HookThread &HookThread::instance()
{
    static HookThread thread;
    return thread;
}

HookThread::HookThread()
{
    std::promise<unsigned long> started;
    std::future<unsigned long> threadId = started.get_future();
    m_thread = std::thread(&HookThread::loop, this, std::move(started));
    m_threadId = threadId.get();
}

HookThread::~HookThread()
{
    PostThreadMessageW(m_threadId, WM_QUIT, 0, 0);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void HookThread::loop(std::promise<unsigned long> started)
{
    MSG message{};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    m_running.store(true);
    started.set_value(GetCurrentThreadId());

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == kRunTasks) {
            drainTasks();
        }
    }
    m_running.store(false);
    m_handlers.clear();
    releaseUnusedHooks();
    drainTasks();
}

void HookThread::execute(std::function<void()> task)
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

void HookThread::drainTasks()
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

Result<void> HookThread::ensureHook(HookKind kind)
{
    void *&hook = kind == HookKind::Keyboard ? m_keyboardHook : m_mouseHook;
    if (hook) {
        return {};
    }
    hook = kind == HookKind::Keyboard ? SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProc, nullptr, 0)
                                      : SetWindowsHookExW(WH_MOUSE_LL, mouseProc, nullptr, 0);
    if (!hook) {
        return Error::fromLastError("SetWindowsHookExW");
    }
    return {};
}

void HookThread::releaseUnusedHooks()
{
    const auto uses = [this](HookKind kind) {
        return std::any_of(m_handlers.begin(), m_handlers.end(), [kind](const Handler &h) { return h.kind == kind; });
    };
    if (m_keyboardHook && !uses(HookKind::Keyboard)) {
        UnhookWindowsHookEx(static_cast<HHOOK>(m_keyboardHook));
        m_keyboardHook = nullptr;
    }
    if (m_mouseHook && !uses(HookKind::Mouse)) {
        UnhookWindowsHookEx(static_cast<HHOOK>(m_mouseHook));
        m_mouseHook = nullptr;
    }
}

Result<int> HookThread::addHandler(HookKind kind, HookGroup group, HookHandler handler)
{
    Result<int> result = 0;
    execute([&] {
        if (Result<void> hooked = ensureHook(kind); !hooked) {
            result = hooked.error();
            return;
        }
        const int id = m_nextId++;
        m_handlers.push_back(Handler{id, kind, group, std::move(handler)});
        result = id;
    });
    return result;
}

void HookThread::removeHandler(int id)
{
    execute([&] {
        std::erase_if(m_handlers, [id](const Handler &h) { return h.id == id; });
        releaseUnusedHooks();
    });
}

void HookThread::removeGroup(HookGroup group)
{
    execute([&] {
        std::erase_if(m_handlers, [group](const Handler &h) { return h.group == group; });
        releaseUnusedHooks();
    });
}

bool HookThread::dispatch(const HookEvent &event)
{
    const std::vector<Handler> handlers = m_handlers;
    bool swallow = false;
    for (const Handler &handler : handlers) {
        if (handler.kind == event.kind && handler.callback && handler.callback(event)) {
            swallow = true;
        }
    }
    return swallow;
}

}  // namespace skyinput
