#include "app/sky_window_watcher.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <QMetaObject>
#include <QTimer>

#include "skyinput/window_info.hpp"

namespace sky {

namespace {

constexpr int kPollMs = 1000;
SkyWindowWatcher *g_watcher = nullptr;

void CALLBACK onForegroundChanged(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD)
{
    if (g_watcher) {
        QMetaObject::invokeMethod(g_watcher, [] {
            if (g_watcher) {
                g_watcher->refresh();
            }
        }, Qt::QueuedConnection);
    }
}

}  // namespace

SkyWindowWatcher::SkyWindowWatcher(QObject *parent) : QObject(parent), m_poll(new QTimer(this))
{
    g_watcher = this;
    m_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_MINIMIZEEND, nullptr, onForegroundChanged, 0, 0,
                             WINEVENT_OUTOFCONTEXT);
    m_poll->setInterval(kPollMs);
    connect(m_poll, &QTimer::timeout, this, &SkyWindowWatcher::refresh);
    m_poll->start();
    m_state = detect();
}

SkyWindowWatcher::~SkyWindowWatcher()
{
    if (m_hook) {
        UnhookWinEvent(static_cast<HWINEVENTHOOK>(m_hook));
    }
    if (g_watcher == this) {
        g_watcher = nullptr;
    }
}

SkyWindowState SkyWindowWatcher::detect() const
{
    const auto handle = reinterpret_cast<HWND>(skyinput::findWindow(L"Sky"));
    if (!handle || !IsWindow(handle)) {
        return SkyWindowState::NotRunning;
    }
    if (IsIconic(handle)) {
        return SkyWindowState::Minimized;
    }
    const HWND foreground = GetForegroundWindow();
    if (foreground == handle || GetAncestor(foreground, GA_ROOTOWNER) == handle) {
        return SkyWindowState::Foreground;
    }
    return SkyWindowState::Background;
}

void SkyWindowWatcher::refresh()
{
    const SkyWindowState next = detect();
    if (next != m_state) {
        m_state = next;
        emit stateChanged(next);
    }
}

}  // namespace sky
