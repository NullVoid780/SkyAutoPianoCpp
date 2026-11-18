#pragma once

#include <memory>
#include <string>
#include <vector>

#include <QObject>

#include "skyinput/global_hotkey.hpp"

namespace sky {

class ConfigService;

enum class ShortcutAction { Previous, PlayPause, Next, IncreaseSpeed, DecreaseSpeed };

inline constexpr double kMinSpeed = 0.1;
inline constexpr double kMaxSpeed = 5.0;

class ShortcutService final : public QObject {
    Q_OBJECT

public:
    explicit ShortcutService(ConfigService &config, QObject *parent = nullptr);
    ~ShortcutService() override;

    void registerAll();
    void unregisterAll();
    void suspend();
    void resume();
    bool isSuspended() const { return m_suspendCount > 0; }
    std::size_t registeredCount() const { return m_hotkeys.size(); }

    static double steppedSpeed(double speed, bool increase);

signals:
    void triggered(sky::ShortcutAction action);
    void speedChanged(double speed);
    void registrationFailed(const QString &accelerator, const QString &reason);

private:
    void handle(ShortcutAction action);
    void changeSpeed(bool increase);

    ConfigService &m_config;
    std::vector<std::unique_ptr<skyinput::GlobalHotkey>> m_hotkeys;
    int m_suspendCount = 0;
};

}  // namespace sky

Q_DECLARE_METATYPE(sky::ShortcutAction)
