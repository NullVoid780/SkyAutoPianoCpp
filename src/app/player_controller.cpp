#include "app/player_controller.hpp"

#include <QTimer>

#include <algorithm>
#include <cmath>
#include <set>

#include "core/translator.hpp"
#include "services/settings/config_service.hpp"
#include "services/library/library_service.hpp"
#include "services/playback/playback_service.hpp"
#include "services/library/recent_service.hpp"
#include "services/notifications/toast_service.hpp"

namespace sky {

PlayerController::PlayerController(LibraryService &library, PlaybackService &playback, RecentService &recent,
                                   ConfigService &config, ToastService &toasts, QObject *parent)
    : QObject(parent),
      m_library(library),
      m_playback(playback),
      m_recent(recent),
      m_config(config),
      m_toasts(toasts),
      m_loopTimer(new QTimer(this))
{
    qRegisterMetaType<sky::LoopMode>();
    m_loopTimer->setSingleShot(true);
    connect(m_loopTimer, &QTimer::timeout, this, [this] {
        if (!m_manualStop) {
            togglePlay();
        }
    });
    connect(&m_playback, &PlaybackService::stopped, this, &PlayerController::onStopped);
    connect(&m_playback, &PlaybackService::progress, this, &PlayerController::onProgress);
    connect(&m_library, &LibraryService::changed, this, &PlayerController::onLibraryChanged);
    connect(&m_library, &LibraryService::payloadChanged, this, [this](int index) {
        if (m_target.kind == PlaybackTarget::Kind::Local && m_target.index == index && !isPlaying()) {
            selectLocal(index, false);
        }
    });
}

void PlayerController::setVisibleIndicesProvider(VisibleIndices provider)
{
    m_visible = std::move(provider);
}

double PlayerController::loopDelaySeconds() const
{
    const double seconds = ConfigService::safeDelayNextMs(m_config.panel()) / 1000.0;
    return seconds == 0.0 ? 0.5 : seconds;
}

void PlayerController::setPlaying(bool playing)
{
    if (m_playing == playing) {
        return;
    }
    m_playing = playing;
    emit playingChanged(playing);
}

void PlayerController::setPosition(int seconds)
{
    m_position = std::max(0, seconds);
    emit positionChanged(m_position);
}

void PlayerController::applyTarget(PlaybackTarget target)
{
    target.totalSeconds = static_cast<int>(std::trunc(target.keyMap.lastTime() / 1000.0));
    m_target = std::move(target);
    setPosition(0);
    emit targetChanged();
}

void PlayerController::selectLocal(int index, bool trackRecentPlay, std::function<void()> then)
{
    const SheetEntry *entry = m_library.entry(index);
    if (!entry) {
        return;
    }
    const quint64 token = ++m_selectionToken;
    const std::string key = entry->payloadFile();
    m_library.loadPayloadAsync(index, [this, token, key, trackRecentPlay, then](auto loaded) {
        if (token != m_selectionToken) {
            return;
        }
        const int current = m_library.indexOfPayload(key);
        const SheetEntry *row = m_library.entry(current);
        if (!row) {
            return;
        }
        if (!loaded) {
            m_toasts.show(ToastType::Error, trUtf8("Error loading song data."));
            return;
        }
        const StoredSheet &payload = *loaded.value();
        PlaybackTarget target;
        target.kind = PlaybackTarget::Kind::Local;
        target.index = current;
        target.payloadKey = key;
        target.name = row->name();
        target.hasHoldNotes = row->hasHoldNotes() || payload.hasHoldNotes;
        target.keyMap = payload.keyMap;
        if (payload.hasHoldNotes && payload.playbackNotes) {
            target.notes = *payload.playbackNotes;
        }
        applyTarget(std::move(target));
        if (trackRecentPlay) {
            m_recent.add(row->name());
        }
        if (then) {
            then();
        }
    });
}

void PlayerController::setRemoteTarget(const std::string &name, KeyMap keyMap, std::vector<PlaybackNote> notes)
{
    ++m_selectionToken;
    PlaybackTarget target;
    target.kind = PlaybackTarget::Kind::Remote;
    target.name = name;
    target.hasHoldNotes = !notes.empty();
    target.keyMap = std::move(keyMap);
    target.notes = std::move(notes);
    applyTarget(std::move(target));
}

void PlayerController::clearTarget()
{
    ++m_selectionToken;
    m_target = PlaybackTarget{};
    setPosition(0);
    emit targetChanged();
}

void PlayerController::togglePlay()
{
    if (m_target.kind == PlaybackTarget::Kind::None) {
        return;
    }
    if (m_playing) {
        m_manualStop = true;
        m_loopTimer->stop();
    }
    const bool play = !m_playing;
    if (play) {
        m_manualStop = false;
        setPlaying(true);
        PlayRequest request;
        request.keyMap = m_target.keyMap.filteredFrom(m_position * 1000.0);
        request.notes = m_target.notes;
        request.startSec = m_position;
        m_playback.play(request);
    } else {
        setPlaying(false);
        m_playback.stop();
    }
}

void PlayerController::onStopped(bool manual)
{
    m_loopTimer->stop();
    setPlaying(false);
    if (manual || m_manualStop) {
        m_manualStop = false;
        return;
    }
    setPosition(0);
    const int delayMs = static_cast<int>(loopDelaySeconds() * 1000);
    if (m_loopMode == LoopMode::All) {
        next();
        m_loopTimer->start(delayMs);
    } else if (m_loopMode == LoopMode::One) {
        m_loopTimer->start(delayMs);
    }
}

void PlayerController::onProgress(int second)
{
    setPosition(second);
}

int PlayerController::neighbour(int direction) const
{
    const int size = m_library.size();
    if (size == 0 || !m_visible) {
        return -1;
    }
    const std::vector<int> visibleList = m_visible();
    const std::set<int> visible(visibleList.begin(), visibleList.end());
    int index = m_target.index + direction;
    for (int searched = 0; searched < size; ++searched, index += direction) {
        if (index < 0) {
            index = size - 1;
        } else if (index >= size) {
            index = 0;
        }
        if (visible.contains(index)) {
            return index;
        }
    }
    return -1;
}

void PlayerController::previous()
{
    if (m_target.kind != PlaybackTarget::Kind::Local) {
        return;
    }
    const int index = neighbour(-1);
    if (index < 0) {
        return;
    }
    selectLocal(index, true, [this] {
        if (!m_playing) {
            return;
        }
        togglePlay();
        setPosition(0);
        QTimer::singleShot(static_cast<int>(loopDelaySeconds() * 1000), this, &PlayerController::togglePlay);
    });
}

void PlayerController::next()
{
    if (m_target.kind != PlaybackTarget::Kind::Local) {
        return;
    }
    const int index = neighbour(1);
    if (index < 0) {
        return;
    }
    selectLocal(index, true, [this] {
        if (!m_playing) {
            return;
        }
        togglePlay();
        setPosition(0);
        m_loopTimer->start(static_cast<int>(loopDelaySeconds() * 1000));
    });
}

void PlayerController::seek(int seconds)
{
    setPosition(std::clamp(seconds, 0, m_target.totalSeconds));
}

void PlayerController::cycleLoopMode()
{
    m_loopMode = m_loopMode == LoopMode::Off ? LoopMode::All : m_loopMode == LoopMode::All ? LoopMode::One : LoopMode::Off;
    emit loopModeChanged(m_loopMode);
}

void PlayerController::onLibraryChanged()
{
    if (m_target.kind != PlaybackTarget::Kind::Local) {
        return;
    }
    const int index = m_library.indexOfPayload(m_target.payloadKey);
    if (index >= 0) {
        m_target.index = index;
        if (const SheetEntry *row = m_library.entry(index); row && row->name() != m_target.name) {
            m_target.name = row->name();
            emit targetChanged();
        }
        return;
    }
    if (m_library.size() > 0) {
        selectLocal(std::clamp(m_target.index, 0, m_library.size() - 1), false);
    } else {
        clearTarget();
    }
}

}  // namespace sky
