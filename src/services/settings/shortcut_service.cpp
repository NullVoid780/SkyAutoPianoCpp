#include "services/settings/shortcut_service.hpp"

#include <QMetaObject>

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/logger.hpp"
#include "core/string_utils.hpp"
#include "services/settings/config_service.hpp"
#include "services/settings/shortcut_parser.hpp"

namespace sky {

ShortcutService::ShortcutService(ConfigService &config, QObject *parent) : QObject(parent), m_config(config)
{
    qRegisterMetaType<sky::ShortcutAction>();
}

ShortcutService::~ShortcutService()
{
    unregisterAll();
}

double ShortcutService::steppedSpeed(double speed, bool increase)
{
    const double current = std::isfinite(speed) ? speed : 1.0;
    const double stepped = std::floor((current + (increase ? 0.1 : -0.1)) * 10 + 0.5) / 10;
    return increase ? std::min(kMaxSpeed, stepped) : std::max(kMinSpeed, stepped);
}

void ShortcutService::registerAll()
{
    if (isSuspended()) {
        return;
    }
    unregisterAll();
    const ShortcutConfig shortcuts = m_config.shortcuts();
    const std::pair<const std::string *, ShortcutAction> bindings[] = {
        {&shortcuts.pre, ShortcutAction::Previous},
        {&shortcuts.play, ShortcutAction::PlayPause},
        {&shortcuts.next, ShortcutAction::Next},
        {&shortcuts.increaseSpeed, ShortcutAction::IncreaseSpeed},
        {&shortcuts.decreaseSpeed, ShortcutAction::DecreaseSpeed},
    };
    for (const auto &[accelerator, action] : bindings) {
        auto parsed = parseShortcut(*accelerator);
        if (!parsed) {
            Logger::warning("shortcut", parsed.error().userMessage);
            emit registrationFailed(toQString(*accelerator), toQString(parsed.error().userMessage));
            continue;
        }
        const ShortcutAction bound = action;
        auto hotkey = skyinput::GlobalHotkey::create(parsed.value().key, parsed.value().modifiers, [this, bound] {
            QMetaObject::invokeMethod(this, [this, bound] { handle(bound); }, Qt::QueuedConnection);
        });
        if (!hotkey) {
            Logger::warning("shortcut", hotkey.error().message());
            emit registrationFailed(toQString(*accelerator), toQString(hotkey.error().message()));
            continue;
        }
        m_hotkeys.push_back(std::move(hotkey).value());
    }
}

void ShortcutService::unregisterAll()
{
    m_hotkeys.clear();
}

void ShortcutService::suspend()
{
    ++m_suspendCount;
    unregisterAll();
}

void ShortcutService::resume()
{
    if (m_suspendCount > 0) {
        --m_suspendCount;
    }
    if (!isSuspended()) {
        registerAll();
    }
}

void ShortcutService::handle(ShortcutAction action)
{
    if (isSuspended()) {
        return;
    }
    Logger::debug("shortcut", "fired " + std::to_string(static_cast<int>(action)));
    if (action == ShortcutAction::IncreaseSpeed || action == ShortcutAction::DecreaseSpeed) {
        changeSpeed(action == ShortcutAction::IncreaseSpeed);
    }
    emit triggered(action);
}

void ShortcutService::changeSpeed(bool increase)
{
    const double speed = steppedSpeed(m_config.panel().speed, increase);
    OrderedJson partial = OrderedJson::object();
    partial["speed"] = jsonNumber(speed);
    m_config.updatePanel(partial);
    emit speedChanged(speed);
}

}  // namespace sky
