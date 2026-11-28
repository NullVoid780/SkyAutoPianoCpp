#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <QObject>

#include "model/key_map.hpp"
#include "model/loop_mode.hpp"
#include "model/note.hpp"

class QTimer;

namespace sky {

class ConfigService;
class IPlaybackPort;
class LibraryService;
class RecentService;
class ToastService;

struct PlaybackTarget {
    enum class Kind { None, Local, Remote };

    Kind kind = Kind::None;
    int index = -1;
    std::string payloadKey;
    std::string name;
    bool hasHoldNotes = false;
    KeyMap keyMap;
    std::vector<PlaybackNote> notes;
    int totalSeconds = 0;
};

class PlayerController final : public QObject {
    Q_OBJECT

public:
    using VisibleIndices = std::function<std::vector<int>()>;

    PlayerController(LibraryService &library, IPlaybackPort &playback, RecentService &recent,
                     ConfigService &config, ToastService &toasts, QObject *parent = nullptr);

    void setVisibleIndicesProvider(VisibleIndices provider);

    void selectLocal(int index, bool trackRecentPlay, std::function<void()> then = {});
    void setRemoteTarget(const std::string &name, KeyMap keyMap, std::vector<PlaybackNote> notes);
    void togglePlay();
    void previous();
    void next();
    void seek(int seconds);
    void cycleLoopMode();
    void clearTarget();

    const PlaybackTarget &target() const { return m_target; }
    bool isPlaying() const { return m_playing; }
    int position() const { return m_position; }
    LoopMode loopMode() const { return m_loopMode; }

signals:
    void targetChanged();
    void playingChanged(bool playing);
    void positionChanged(int seconds);
    void loopModeChanged(sky::LoopMode mode);

private:
    void onStopped(bool manual);
    void onProgress(int second);
    void onLibraryChanged();
    void setPlaying(bool playing);
    void setPosition(int seconds);
    void applyTarget(PlaybackTarget target);
    int neighbour(int direction) const;
    double loopDelaySeconds() const;

    LibraryService &m_library;
    IPlaybackPort &m_playback;
    RecentService &m_recent;
    ConfigService &m_config;
    ToastService &m_toasts;
    VisibleIndices m_visible;
    PlaybackTarget m_target;
    QTimer *m_loopTimer;
    bool m_playing = false;
    bool m_manualStop = false;
    int m_position = 0;
    LoopMode m_loopMode = LoopMode::Off;
    quint64 m_selectionToken = 0;
};

}  // namespace sky
