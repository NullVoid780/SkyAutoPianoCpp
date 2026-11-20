#include "ui/main_window/player_footer.hpp"

#include "ui/main_window/mode_switch.hpp"
#include "ui/main_window/sky_status_chip.hpp"

#include <QLabel>
#include <QPainter>
#include <QSignalBlocker>
#include <QSlider>

#include "core/time_utils.hpp"
#include "core/string_utils.hpp"
#include "ui/main_window/loop_control.hpp"
#include "ui/main_window/speed_control.hpp"
#include "ui/main_window/theme_toggle.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/badge.hpp"
#include "ui/widgets/icon_button.hpp"
#include "ui/widgets/inline_switch.hpp"
#include "ui/widgets/seek_slider.hpp"
#include "ui/widgets/transport_button.hpp"

namespace sky {

namespace {

constexpr int kCorner = 10;
constexpr int kIcon = 20;
constexpr int kNameGap = 15;
constexpr int kSliderMargin = 10;
constexpr int kSliderHeight = 21;
constexpr int kTransportGap = 6;
constexpr int kButtonsGap = 12;
constexpr int kLoopLeft = 30;
constexpr int kLoopBottom = 12;
constexpr int kSpeedRight = 25;
constexpr int kSpeedTop = 35;
constexpr int kBadgeGap = 8;

QString clock(int seconds)
{
    return toQString(formatClock(seconds));
}

}  // namespace

PlayerFooter::PlayerFooter(QWidget *parent)
    : QWidget(parent),
      m_gear(new IconButton(QStringLiteral("gear-fill"), kIcon, QSize(kIcon, kIcon),
                            IconButtonStyle{[] { return currentTheme().footerText; }, [] { return currentTheme().hoverColor; },
                                            {}, {}, 0, kGearSpinDegrees, kHoverSlowMs},
                            this)),
      m_theme(new ThemeToggle(this)),
      m_name(new QLabel(this)),
      m_hold(new Badge(QStringLiteral("Hold"), this)),
      m_progress(new SeekSlider(this)),
      m_elapsed(new QLabel(QStringLiteral("00:00"), this)),
      m_total(new QLabel(QStringLiteral("00:00"), this)),
      m_longPress(new InlineSwitch(QStringLiteral("Long-press mode: "), this)),
      m_loop(new LoopControl(this)),
      m_previous(new TransportButton(TransportButton::Kind::Skip, QStringLiteral("skip-start-fill"), this)),
      m_play(new TransportButton(TransportButton::Kind::Play, QStringLiteral("play-fill"), this)),
      m_next(new TransportButton(TransportButton::Kind::Skip, QStringLiteral("skip-end-fill"), this)),
      m_speed(new SpeedControl(this)),
      m_sky(new SkyStatusChip(this)),
      m_mode(new ModeSwitch({QStringLiteral("Auto"), QStringLiteral("Practice")}, this))
{
    m_mode->setToolTip(QStringLiteral("Auto plays the song for you. Practice shows a guide over Sky and you play it."));
    m_mode->setAccessibleName(QStringLiteral("Play mode"));
    connect(m_mode, &ModeSwitch::chosen, this, [this](int index) { emit practiceModeChosen(index == 1); });
    setObjectName(QStringLiteral("PlayerFooter"));
    m_name->setObjectName(QStringLiteral("NowPlaying"));
    m_elapsed->setObjectName(QStringLiteral("TimeLabel"));
    m_total->setObjectName(QStringLiteral("TimeLabel"));
    m_progress->setObjectName(QStringLiteral("ProgressSlider"));
    m_progress->setRange(0, 0);
    m_hold->hide();

    connect(m_gear, &QAbstractButton::clicked, this, &PlayerFooter::settingsRequested);
    connect(m_theme, &QAbstractButton::clicked, this, &PlayerFooter::themeToggleRequested);
    connect(m_previous, &QAbstractButton::clicked, this, &PlayerFooter::previousRequested);
    connect(m_play, &QAbstractButton::clicked, this, &PlayerFooter::playRequested);
    connect(m_next, &QAbstractButton::clicked, this, &PlayerFooter::nextRequested);
    connect(m_progress, &QSlider::valueChanged, this, [this](int value) {
        m_elapsed->setText(clock(value));
        emit seekRequested(value);
    });
    connect(m_longPress, &QAbstractButton::toggled, this, &PlayerFooter::longPressToggled);
    connect(m_speed, &SpeedControl::speedCommitted, this, &PlayerFooter::speedCommitted);
    connect(m_loop, &LoopControl::cycleRequested, this, &PlayerFooter::loopCycleRequested);
    connect(m_loop, &LoopControl::delayChanged, this, &PlayerFooter::delayChanged);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });

    setShortcutLabels(QStringLiteral("Ctrl+Shift+C"), QStringLiteral("Ctrl+Shift+V"), QStringLiteral("Ctrl+Shift+B"));
    showEmpty();
}

void PlayerFooter::refreshTransportLabels()
{
    m_previous->setContent(QStringLiteral("skip-start-fill"), QStringLiteral("Prev (%1)").arg(m_previousShortcut));
    QString playLabel = m_playing ? QStringLiteral("Pause") : QStringLiteral("Play");
    if (m_practice) {
        playLabel = m_playing ? QStringLiteral("Stop") : QStringLiteral("Practice");
    }
    m_play->setContent(m_playing ? QStringLiteral("pause-fill") : QStringLiteral("play-fill"),
                       QStringLiteral("%1 (%2)").arg(playLabel, m_playShortcut));
    m_next->setContent(QStringLiteral("skip-end-fill"), QStringLiteral("Next (%1)").arg(m_nextShortcut));
    relayout();
}

void PlayerFooter::setShortcutLabels(const QString &previous, const QString &play, const QString &next)
{
    m_previousShortcut = previous;
    m_playShortcut = play;
    m_nextShortcut = next;
    refreshTransportLabels();
}

void PlayerFooter::showTarget(const QString &name, bool hold, int totalSeconds)
{
    m_name->setText(name);
    m_hold->setVisible(hold);
    {
        const QSignalBlocker blocker(m_progress);
        m_progress->setRange(0, totalSeconds);
        m_progress->setValue(0);
    }
    m_elapsed->setText(clock(0));
    m_total->setText(clock(totalSeconds));
    relayout();
}

void PlayerFooter::showEmpty()
{
    m_name->setText(QStringLiteral("* Click on the plus to add sheet"));
    m_hold->hide();
    {
        const QSignalBlocker blocker(m_progress);
        m_progress->setRange(0, 0);
        m_progress->setValue(0);
    }
    m_elapsed->setText(clock(0));
    m_total->setText(clock(0));
    relayout();
}

void PlayerFooter::setPlaying(bool playing)
{
    m_playing = playing;
    m_progress->setEnabled(!playing);
    refreshTransportLabels();
}

void PlayerFooter::setPosition(int seconds)
{
    const QSignalBlocker blocker(m_progress);
    m_progress->setValue(seconds);
    m_elapsed->setText(clock(seconds));
}

void PlayerFooter::setSpeed(double speed)
{
    m_speed->setSpeed(speed);
}

void PlayerFooter::setLongPress(bool enabled)
{
    m_longPress->setCheckedSilently(enabled);
}

void PlayerFooter::setLoopMode(LoopMode mode)
{
    m_loop->setMode(mode);
}

void PlayerFooter::setDelaySeconds(int seconds)
{
    m_loop->setDelaySeconds(seconds);
}

void PlayerFooter::setPracticeMode(bool practice)
{
    m_practice = practice;
    m_mode->setCurrent(practice ? 1 : 0);
    refreshTransportLabels();
}

void PlayerFooter::setSkyStatus(const QString &text, int tone, const QString &tip)
{
    m_sky->setStatus(text, static_cast<SkyStatusChip::Tone>(tone), tip);
    relayout();
}

bool PlayerFooter::isEditingSpeed() const
{
    return m_speed->isEditing();
}

void PlayerFooter::resizeEvent(QResizeEvent *)
{
    relayout();
}

void PlayerFooter::relayout()
{
    const int w = width();
    m_gear->move(kCorner, kCorner);
    m_theme->move(w - kCorner - m_theme->width(), kCorner);
    m_mode->resize(m_mode->sizeHint());
    m_mode->move(kCorner + m_gear->width() + kCorner, kCorner + (m_gear->height() - m_mode->height()) / 2);
    m_sky->resize(m_sky->sizeHint());
    m_sky->move(m_theme->x() - kCorner - m_sky->width(), kCorner + (m_theme->height() - m_sky->height()) / 2);
    const int cornerSpace = std::max(m_mode->x() + m_mode->width(), w - m_sky->x()) + kCorner;

    m_name->adjustSize();
    const int nameRowHeight = std::max(m_name->height(), m_hold->isVisible() ? m_hold->height() : 0);
    const int timeHeight = m_elapsed->sizeHint().height();
    const int buttonsHeight = m_play->sizeHint().height();
    const int visibleHeight = kHeight - kOverhang;
    const int columnHeight = nameRowHeight + kNameGap + kSliderMargin * 2 + 3 + timeHeight + kButtonsGap + buttonsHeight;
    int y = (visibleHeight - columnHeight) / 2;

    const int nameWidth = std::max(80, std::min(m_name->width(), w - 2 * cornerSpace));
    const int rowWidth = nameWidth + (m_hold->isVisible() ? kBadgeGap + m_hold->width() : 0);
    int x = (w - rowWidth) / 2;
    m_name->setGeometry(x, y + (nameRowHeight - m_name->height()) / 2, nameWidth, m_name->height());
    m_hold->move(x + nameWidth + kBadgeGap, y + (nameRowHeight - m_hold->height()) / 2);
    y += nameRowHeight + kNameGap;

    const int barWidth = static_cast<int>(w * 0.9);
    const int barLeft = (w - barWidth) / 2;
    const int trackCenter = y + kSliderMargin + 1;
    m_progress->setGeometry(barLeft, trackCenter - kSliderHeight / 2, barWidth, kSliderHeight);
    y += kSliderMargin * 2 + 3;

    m_elapsed->setGeometry(barLeft, y, barWidth / 2, timeHeight);
    m_total->setGeometry(barLeft + barWidth / 2, y, barWidth - barWidth / 2, timeHeight);
    m_total->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    const QSize longPress = m_longPress->sizeHint();
    m_longPress->setGeometry((w - longPress.width()) / 2, y + (timeHeight - longPress.height()) / 2, longPress.width(),
                             longPress.height());
    y += timeHeight + kButtonsGap;

    const QSize previous = m_previous->sizeHint();
    const QSize play = m_play->sizeHint();
    const QSize next = m_next->sizeHint();
    const int buttonsWidth = previous.width() + play.width() + next.width() + kTransportGap * 2;
    x = (w - buttonsWidth) / 2;
    m_previous->setGeometry(QRect(QPoint(x, y), previous));
    x += previous.width() + kTransportGap;
    m_play->setGeometry(QRect(QPoint(x, y), play));
    x += play.width() + kTransportGap;
    m_next->setGeometry(QRect(QPoint(x, y), next));

    const QSize loop = m_loop->sizeHint();
    m_loop->setGeometry(kLoopLeft, visibleHeight - kLoopBottom - loop.height(), loop.width(), loop.height());

    const QSize speed = m_speed->sizeHint();
    m_speed->setGeometry(w - kSpeedRight - speed.width(), kSpeedTop, speed.width(), speed.height());
}

void PlayerFooter::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    painter.fillRect(rect(), theme.footerFill);
    painter.fillRect(QRect(0, 0, width(), 1), theme.footerTopEdge);
}

}  // namespace sky
