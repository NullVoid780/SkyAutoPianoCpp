#include "ui/practice/play_along.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <QGuiApplication>
#include <QMetaObject>
#include <QScreen>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cmath>

#include "core/string_utils.hpp"
#include "model/key_table.hpp"
#include "services/settings/config_service.hpp"
#include "services/settings/ui_state_service.hpp"
#include "skyinput/low_level_hook.hpp"
#include "skyinput/window_info.hpp"
#include "ui/practice/play_along_overlay.hpp"
#include "ui/practice/sky_calibrator.hpp"
#include "ui/settings_window/key_capture.hpp"

namespace sky {

namespace {

constexpr int kFrameMs = 16;
constexpr double kMaxFrameMs = 50;
constexpr double kCountdownMs = 3000;
constexpr double kMinScale = 0.35;
constexpr double kMaxScale = 2.5;
constexpr double kScaleStep = 1.04;
constexpr int kBarTop = 12;
constexpr int kMessageMs = 3500;
constexpr int kResultMs = 2600;
constexpr double kPromptBottom = 0.03;
constexpr std::array<double, 3> kSpeeds = {1.0, 0.75, 0.5};
constexpr std::array<int, 5> kOpacities = {100, 85, 70, 55, 40};
const char *kLayoutKey = "playAlongGuide";

QPoint toLogical(const QPoint &physical)
{
    for (QScreen *screen : QGuiApplication::screens()) {
        const QRect logical = screen->geometry();
        const qreal ratio = screen->devicePixelRatio();
        const QRect native(logical.topLeft(), logical.size() * ratio);
        if (native.contains(physical)) {
            const QPoint offset = physical - logical.topLeft();
            return logical.topLeft() + QPoint(qRound(offset.x() / ratio), qRound(offset.y() / ratio));
        }
    }
    return physical;
}

}  // namespace

PlayAlong::PlayAlong(ConfigService &config, UiStateService &uiState, QObject *parent)
    : QObject(parent),
      m_config(config),
      m_uiState(uiState),
      m_timer(new QTimer(this)),
      m_calibrator(new SkyCalibrator(this))
{
    connect(m_calibrator, &SkyCalibrator::instruction, this, [this](const QString &text) {
        if (m_bar) {
            m_bar->setMessage(text);
        }
    });
    connect(m_calibrator, &SkyCalibrator::finished, this, &PlayAlong::finishAlign);
    connect(m_calibrator, &SkyCalibrator::prompt, this, [this](int step, const QString &key, const QString &note) {
        if (!m_prompt) {
            m_prompt = new AlignPrompt();
        }
        m_prompt->showStep(step, key, note);
        placePrompt();
        m_prompt->show();
    });
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(kFrameMs);
    connect(m_timer, &QTimer::timeout, this, &PlayAlong::tick);
    m_clock.start();
    loadLayout();
}

PlayAlong::~PlayAlong()
{
    removeHooks();
    delete m_overlay;
    delete m_bar;
    delete m_prompt;
}

void PlayAlong::start(const QString &name, const KeyMap &keyMap)
{
    if (!m_overlay) {
        m_overlay = new PlayAlongOverlay();
        m_overlay->stage()->setSession(&m_session);
        connect(m_overlay, &PlayAlongOverlay::dragged, this, [this](const QPoint &delta) {
            const QRect anchor = anchorRect();
            m_anchorX = std::clamp(m_anchorX + delta.x() / static_cast<double>(std::max(1, anchor.width())), 0.1, 0.9);
            m_anchorY = std::clamp(m_anchorY + delta.y() / static_cast<double>(std::max(1, anchor.height())), 0.1, 0.9);
            place();
        });
        connect(m_overlay, &PlayAlongOverlay::windowEdited, this, [this](const QRect &geometry) {
            m_floatingRect = geometry;
            place();
        });
        connect(m_overlay, &PlayAlongOverlay::windowEditFinished, this, &PlayAlong::saveLayout);
        connect(m_overlay, &PlayAlongOverlay::wheelResized, this, [this](int steps) {
            const double factor = std::pow(kScaleStep, steps);
            m_scaleX = std::clamp(m_scaleX * factor, kMinScale, kMaxScale);
            m_scaleY = std::clamp(m_scaleY * factor, kMinScale, kMaxScale);
            place();
        });
    }
    if (!m_bar) {
        m_bar = new PlayAlongBar();
        connect(m_bar, &PlayAlongBar::waitToggled, this, [this](bool wait) { m_session.setWaitMode(wait); });
        connect(m_bar, &PlayAlongBar::speedClicked, this, &PlayAlong::cycleSpeed);
        connect(m_bar, &PlayAlongBar::opacityClicked, this, &PlayAlong::cycleOpacity);
        connect(m_bar, &PlayAlongBar::moveClicked, this, &PlayAlong::toggleMove);
        connect(m_bar, &PlayAlongBar::floatingClicked, this, &PlayAlong::toggleFloating);
        connect(m_bar, &PlayAlongBar::alignClicked, this, &PlayAlong::toggleAlign);
        connect(m_bar, &PlayAlongBar::restartClicked, this, &PlayAlong::restart);
        connect(m_bar, &PlayAlongBar::stopClicked, this, &PlayAlong::stop);
    }
    m_session = PracticeSession(keyMap);
    m_session.setWaitMode(true);
    m_bar->setTitle(name);
    m_bar->setWait(true);
    m_bar->setSpeed(m_speed);
    m_bar->setOpacityPercent(m_opacity);
    m_bar->setMoving(false);
    m_bar->setFloating(m_floating);
    m_overlay->setFloating(m_floating);
    m_overlay->setWindowOpacity(m_opacity / 100.0);
    refreshLabels();
    m_active = true;
    m_shown = false;
    installHooks();
    restart();
    refreshVisibility();
}

void PlayAlong::stop()
{
    if (!m_active) {
        return;
    }
    m_active = false;
    if (m_calibrator->isRunning()) {
        m_calibrator->cancel();
    }
    m_timer->stop();
    removeHooks();
    if (m_overlay) {
        m_overlay->setMoveMode(false);
        m_overlay->hide();
    }
    if (m_bar) {
        m_bar->hide();
    }
    if (m_prompt) {
        m_prompt->hide();
    }
    saveLayout();
    emit finished();
}

void PlayAlong::setSkyState(SkyWindowState state)
{
    m_sky = state;
    refreshVisibility();
}

void PlayAlong::refreshVisibility()
{
    if (!m_active || !m_overlay || !m_bar) {
        return;
    }
    const bool moving = m_overlay->isMoving();
    const bool show = moving || m_sky == SkyWindowState::Foreground || m_sky == SkyWindowState::NotRunning;
    if (show) {
        place();
        if (!m_bar->isVisible()) {
            m_bar->show();
        }
        if (m_calibrator->isRunning()) {
            m_overlay->hide();
            return;
        }
        if (!m_overlay->isVisible()) {
            m_overlay->show();
        }
        if (!m_shown) {
            m_shown = true;
            restart();
        }
        if (m_focusPaused && m_phase == SkyGuideStage::Phase::Paused) {
            m_phase = SkyGuideStage::Phase::Playing;
            m_last = static_cast<double>(m_clock.elapsed());
        }
        m_focusPaused = false;
        m_timer->start();
    } else {
        if (m_phase == SkyGuideStage::Phase::Playing || m_phase == SkyGuideStage::Phase::Countdown) {
            m_phase = SkyGuideStage::Phase::Paused;
            m_focusPaused = true;
        }
        m_overlay->hide();
        m_bar->hide();
        m_timer->stop();
    }
}

void PlayAlong::restart()
{
    m_session.reset(m_speed);
    if (m_overlay) {
        m_overlay->stage()->clearEffects();
    }
    m_countdown = kCountdownMs;
    m_phase = m_session.empty() ? SkyGuideStage::Phase::Results : SkyGuideStage::Phase::Countdown;
    m_last = static_cast<double>(m_clock.elapsed());
    m_focusPaused = false;
}

void PlayAlong::tick()
{
    if (!m_overlay) {
        return;
    }
    const double now = static_cast<double>(m_clock.elapsed());
    const double delta = std::clamp(now - m_last, 0.0, kMaxFrameMs);
    m_last = now;
    if (m_phase == SkyGuideStage::Phase::Countdown) {
        m_countdown -= delta;
        if (m_countdown <= 0) {
            m_countdown = 0;
            m_phase = SkyGuideStage::Phase::Playing;
        }
    } else if (m_phase == SkyGuideStage::Phase::Playing) {
        for (const PracticeHit &miss : m_session.advance(delta, m_speed)) {
            m_overlay->stage()->addHit(miss, now);
        }
        if (m_session.finished()) {
            m_phase = SkyGuideStage::Phase::Results;
        }
    }
    if (static_cast<int>(now) % 200 < kFrameMs) {
        place();
    }
    m_overlay->stage()->setFrame(m_phase, now, m_speed, m_countdown);
}

void PlayAlong::press(int key)
{
    if (m_calibrator->isRunning()) {
        m_calibrator->keyPressed(key);
        return;
    }
    if (!m_active || !m_overlay || m_phase != SkyGuideStage::Phase::Playing) {
        return;
    }
    m_overlay->stage()->addHit(m_session.press(key, m_speed), static_cast<double>(m_clock.elapsed()));
}

QRect PlayAlong::anchorRect() const
{
    const auto handle = reinterpret_cast<HWND>(skyinput::findWindow(L"Sky"));
    if (handle && IsWindow(handle) && !IsIconic(handle)) {
        RECT client{};
        GetClientRect(handle, &client);
        POINT topLeft{client.left, client.top};
        POINT bottomRight{client.right, client.bottom};
        ClientToScreen(handle, &topLeft);
        ClientToScreen(handle, &bottomRight);
        const QPoint a = toLogical(QPoint(topLeft.x, topLeft.y));
        const QPoint b = toLogical(QPoint(bottomRight.x, bottomRight.y));
        const QRect rect(a, b);
        if (rect.width() > 100 && rect.height() > 100) {
            return rect;
        }
    }
    QScreen *screen = QGuiApplication::primaryScreen();
    return screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
}

void PlayAlong::place()
{
    if (!m_overlay || !m_bar) {
        return;
    }
    const QRect anchor = anchorRect();
    QPoint barPos;
    m_bar->adjustSize();
    if (m_floating) {
        if (!m_floatingRect.isValid()) {
            m_floatingRect = defaultFloatingRect(anchor);
        }
        if (m_overlay->geometry() != m_floatingRect) {
            m_overlay->setGeometry(m_floatingRect);
        }
        m_overlay->stage()->setFitMode(true);
        barPos = QPoint(m_floatingRect.center().x() - m_bar->width() / 2, m_floatingRect.top() - kBarTop / 2 - m_bar->height());
        QScreen *screen = QGuiApplication::screenAt(m_floatingRect.center());
        const QRect available = screen ? screen->availableGeometry() : anchor;
        if (barPos.y() < available.top()) {
            barPos.setY(m_floatingRect.bottom() + kBarTop / 2);
        }
    } else {
        if (m_overlay->geometry() != anchor) {
            m_overlay->setGeometry(anchor);
        }
        m_overlay->stage()->setFitMode(false);
        m_overlay->stage()->setGrid(m_anchorX, m_anchorY, m_scaleX, m_scaleY);
        barPos = QPoint(anchor.center().x() - m_bar->width() / 2, anchor.top() + kBarTop);
    }
    if (m_bar->pos() != barPos) {
        m_bar->move(barPos);
    }
    m_lastAnchor = anchor;
}

void PlayAlong::installHooks()
{
    removeHooks();
    const KeyboardConfig keyboard = m_config.keyboard();
    for (int i = 0; i < kSkyKeyCount; ++i) {
        const std::string name = resolveOutputKey(kAutoKeys[static_cast<std::size_t>(i)], keyboard);
        const auto code = skyinput::keyFromName(name);
        if (!code) {
            continue;
        }
        auto hook = skyinput::LowLevelHook::onKey(*code, true, [this, i] {
            QMetaObject::invokeMethod(this, [this, i] { press(i); }, Qt::QueuedConnection);
        });
        if (hook) {
            m_hooks.push_back(std::move(hook.value()));
        }
    }
}

void PlayAlong::removeHooks()
{
    m_hooks.clear();
}

void PlayAlong::refreshLabels()
{
    if (!m_overlay) {
        return;
    }
    const KeyboardConfig keyboard = m_config.keyboard();
    std::array<QString, kSkyKeyCount> labels;
    for (int i = 0; i < kSkyKeyCount; ++i) {
        const std::string value = resolveOutputKey(kAutoKeys[static_cast<std::size_t>(i)], keyboard);
        labels[static_cast<std::size_t>(i)] = keyboardDisplayLabel(toQString(value)).toUpper();
    }
    m_overlay->stage()->setLabels(labels);
}

void PlayAlong::cycleSpeed()
{
    const auto current = std::find(kSpeeds.begin(), kSpeeds.end(), m_speed);
    m_speed = current == kSpeeds.end() || current + 1 == kSpeeds.end() ? kSpeeds.front() : *(current + 1);
    m_bar->setSpeed(m_speed);
    place();
}

void PlayAlong::cycleOpacity()
{
    const auto current = std::find(kOpacities.begin(), kOpacities.end(), m_opacity);
    m_opacity = current == kOpacities.end() || current + 1 == kOpacities.end() ? kOpacities.front() : *(current + 1);
    m_overlay->setWindowOpacity(m_opacity / 100.0);
    m_bar->setOpacityPercent(m_opacity);
    place();
    saveLayout();
}

void PlayAlong::toggleMove()
{
    const bool moving = !m_overlay->isMoving();
    m_overlay->setMoveMode(moving);
    m_bar->setMoving(moving);
    if (!moving) {
        saveLayout();
    }
    refreshVisibility();
}

void PlayAlong::toggleAlign()
{
    if (m_calibrator->isRunning()) {
        m_calibrator->cancel();
        return;
    }
    const auto sky = reinterpret_cast<HWND>(skyinput::findWindow(L"Sky"));
    if (!sky || !IsWindow(sky) || IsIconic(sky)) {
        flashMessage(QStringLiteral("Open Sky and its instrument first"));
        return;
    }
    if (m_floating) {
        toggleFloating();
    }
    if (m_overlay->isMoving()) {
        toggleMove();
    }
    m_phaseBeforeAlign = m_phase;
    if (m_phase == SkyGuideStage::Phase::Playing || m_phase == SkyGuideStage::Phase::Countdown) {
        m_phase = SkyGuideStage::Phase::Paused;
    }
    m_overlay->hide();
    m_bar->setAligning(true);
    m_calibrator->start(anchorRect());
}

void PlayAlong::finishAlign(bool ok, const SkyGrid &grid, const QString &message)
{
    if (m_bar) {
        m_bar->setAligning(false);
    }
    if (m_prompt) {
        m_prompt->showResult(ok, message);
        placePrompt();
        m_prompt->show();
        QTimer::singleShot(kResultMs, m_prompt, [prompt = m_prompt, calibrator = m_calibrator] {
            if (prompt && !calibrator->isRunning()) {
                prompt->hide();
            }
        });
    }
    if (ok) {
        m_anchorX = grid.centerX;
        m_anchorY = grid.centerY;
        m_scaleX = grid.scaleX;
        m_scaleY = grid.scaleY;
        saveLayout();
    }
    if (!m_active) {
        return;
    }
    if (m_phaseBeforeAlign == SkyGuideStage::Phase::Playing || m_phaseBeforeAlign == SkyGuideStage::Phase::Countdown) {
        m_phase = m_phaseBeforeAlign;
        m_last = static_cast<double>(m_clock.elapsed());
    }
    flashMessage(message);
    refreshVisibility();
}

void PlayAlong::placePrompt()
{
    if (!m_prompt) {
        return;
    }
    const QRect anchor = anchorRect();
    m_prompt->move(anchor.center().x() - m_prompt->width() / 2,
                   anchor.bottom() - m_prompt->height() - static_cast<int>(anchor.height() * kPromptBottom));
}

void PlayAlong::flashMessage(const QString &message)
{
    if (!m_bar) {
        return;
    }
    m_bar->setMessage(message);
    QTimer::singleShot(kMessageMs, m_bar, [bar = m_bar] {
        if (bar) {
            bar->setMessage(QString());
        }
    });
}

QRect PlayAlong::defaultFloatingRect(const QRect &anchor) const
{
    const int width = std::clamp(anchor.width() / 3, 360, 640);
    const int height = width * 3 / 5;
    return QRect(anchor.center().x() - width / 2, anchor.top() + anchor.height() / 6, width, height);
}

void PlayAlong::toggleFloating()
{
    m_floating = !m_floating;
    m_overlay->setFloating(m_floating);
    m_bar->setFloating(m_floating);
    if (m_floating && !m_overlay->isMoving()) {
        toggleMove();
    }
    place();
    saveLayout();
}

void PlayAlong::loadLayout()
{
    const OrderedJson layout = m_uiState.value(kLayoutKey);
    if (!layout.is_object()) {
        return;
    }
    m_anchorX = std::clamp(layout.value("x", m_anchorX), 0.0, 1.0);
    m_anchorY = std::clamp(layout.value("y", m_anchorY), 0.0, 1.0);
    const double scale = layout.value("scale", 1.0);
    m_scaleX = std::clamp(layout.value("sx", scale), kMinScale, kMaxScale);
    m_scaleY = std::clamp(layout.value("sy", scale), kMinScale, kMaxScale);
    m_floating = layout.value("floating", false);
    const OrderedJson window = layout.value("window", OrderedJson::object());
    if (window.is_object() && window.contains("w")) {
        m_floatingRect = QRect(window.value("x", 0), window.value("y", 0), std::max(260, window.value("w", 480)),
                               std::max(160, window.value("h", 300)));
        if (!QGuiApplication::screenAt(m_floatingRect.center())) {
            m_floatingRect = QRect();
        }
    }
    const int opacity = layout.value("opacity", m_opacity);
    if (std::find(kOpacities.begin(), kOpacities.end(), opacity) != kOpacities.end()) {
        m_opacity = opacity;
    }
}

void PlayAlong::saveLayout()
{
    OrderedJson layout{{"x", m_anchorX}, {"y", m_anchorY}, {"sx", m_scaleX}, {"sy", m_scaleY}, {"opacity", m_opacity},
                       {"floating", m_floating}};
    if (m_floatingRect.isValid()) {
        layout["window"] = OrderedJson{{"x", m_floatingRect.x()},
                                       {"y", m_floatingRect.y()},
                                       {"w", m_floatingRect.width()},
                                       {"h", m_floatingRect.height()}};
    }
    m_uiState.setValue(kLayoutKey, std::move(layout));
}

}  // namespace sky
