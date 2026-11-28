#pragma once

#include <QObject>

#include "services/playback/player.hpp"

namespace sky {

class IPlaybackPort : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~IPlaybackPort() override;

    virtual void play(const PlayRequest &request) = 0;
    virtual void stop() = 0;
    virtual bool isPlaying() const = 0;

signals:
    void started();
    void progress(int second);
    void stopped(bool manual);
};

}  // namespace sky
