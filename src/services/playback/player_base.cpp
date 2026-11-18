#include "services/playback/player_base.hpp"

#include <windows.h>

#include <cstring>
#include <exception>
#include <string>

#include "core/logger.hpp"
#include "core/precise_sleep.hpp"
#include "services/settings/config_service.hpp"

namespace sky {

PlayerBase::PlayerBase(ConfigService &config, skyinput::Keyboard &keyboard, QObject *parent)
    : QObject(parent), m_config(config), m_keyboard(keyboard)
{
}

PlayerBase::~PlayerBase()
{
    shutdown();
}

void PlayerBase::shutdown()
{
    {
        std::lock_guard lock(m_stateMutex);
        m_playing = false;
        ++m_session;
    }
    joinWorkers();
}

void PlayerBase::joinWorkers()
{
    if (m_playbackThread.joinable()) {
        m_playbackThread.join();
    }
    if (m_progressThread.joinable()) {
        m_progressThread.join();
    }
}

void PlayerBase::start(const PlayRequest &request)
{
    quint64 session = 0;
    {
        std::lock_guard lock(m_stateMutex);
        session = ++m_session;
        m_playing = false;
    }
    joinWorkers();
    releaseHeldKeys();

    if (request.keyMap.empty() && request.notes.empty()) {
        emit stopped(false);
        return;
    }

    m_playing = true;
    m_playbackThread = std::thread([this, request, session] {
        runGuarded("playback", THREAD_PRIORITY_TIME_CRITICAL, [&] { playback(request, session); });
    });
    m_progressThread = std::thread([this, request, session] {
        runGuarded("progress", THREAD_PRIORITY_ABOVE_NORMAL, [&] { trackProgress(request, session); });
    });
}

void PlayerBase::stop(bool emitStopEvent, bool manual)
{
    bool wasPlaying = false;
    {
        std::lock_guard lock(m_stateMutex);
        wasPlaying = m_playing.exchange(false);
        ++m_session;
    }
    joinWorkers();
    releaseHeldKeys();
    if (emitStopEvent && wasPlaying) {
        emit stopped(manual);
    }
}

bool PlayerBase::isPlaying() const
{
    return m_playing.load();
}

bool PlayerBase::isActive(quint64 session) const
{
    return m_playing.load() && m_session.load() == session;
}

void PlayerBase::finishNaturally(quint64 session)
{
    {
        std::lock_guard lock(m_stateMutex);
        if (m_session.load() != session || !m_playing.load()) {
            return;
        }
        m_playing = false;
    }
    emit stopped(false);
}

void PlayerBase::emitProgress(quint64 session, int second)
{
    if (isActive(session)) {
        emit progress(second);
    }
}

std::optional<skyinput::KeyCode> PlayerBase::keyCodeFor(const std::string &name) const
{
    return skyinput::keyFromName(name);
}

void PlayerBase::runGuarded(const char *name, int priority, const std::function<void()> &body)
{
    SetThreadPriority(GetCurrentThread(), priority);
    SetThreadDescription(GetCurrentThread(), std::wstring(name, name + std::strlen(name)).c_str());
    TimerResolutionScope resolution;
    try {
        body();
    } catch (const std::exception &error) {
        Logger::error(name, error.what());
        releaseHeldKeys();
        std::terminate();
    } catch (...) {
        Logger::error(name, "unknown exception");
        releaseHeldKeys();
        std::terminate();
    }
}

}  // namespace sky
