#include "ui/issue/recording_bar.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

#include "ui/render/paint_utils.hpp"
#include "ui/settings_window/action_button.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kPadding = 8;
constexpr int kSpacing = 8;
constexpr int kTopGap = 12;
constexpr int kDot = 8;
constexpr qreal kRadius = 6;

class RecordingDot final : public QWidget {
public:
    explicit RecordingDot(QWidget *parent) : QWidget(parent) { setFixedSize(kDot + 4, kDot + 4); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        preparePainter(painter);
        painter.setPen(Qt::NoPen);
        painter.setBrush(currentTheme().errorColor);
        painter.drawEllipse(QRectF(2, 2, kDot, kDot));
    }
};

QString clockText(qint64 milliseconds)
{
    const qint64 seconds = milliseconds / 1000;
    return QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

}  // namespace

RecordingBar::RecordingBar()
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint),
      m_time(new QLabel(clockText(0), this)),
      m_steps(new QLabel(this)),
      m_stop(new ActionButton(ActionButton::Kind::Primary, QStringLiteral("check-lg"), QStringLiteral("Stop and save"), this)),
      m_discard(new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("trash3"), QStringLiteral("Discard"), this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowTitle(QStringLiteral("Recording an issue"));
    setCursor(Qt::SizeAllCursor);

    auto *title = new QLabel(QStringLiteral("Recording"), this);
    title->setObjectName(QStringLiteral("RecordingTitle"));
    m_time->setObjectName(QStringLiteral("RecordingTime"));
    m_steps->setObjectName(QStringLiteral("SettingsRowDescription"));
    m_stop->setToolTip(QStringLiteral("Stop recording and save the report"));
    m_discard->setToolTip(QStringLiteral("Throw away this recording"));
    for (QWidget *button : std::initializer_list<QWidget *>{m_stop, m_discard}) {
        button->setCursor(Qt::PointingHandCursor);
    }

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(kPadding + 4, kPadding, kPadding, kPadding);
    row->setSpacing(kSpacing);
    row->addWidget(new RecordingDot(this));
    row->addWidget(title);
    row->addWidget(m_time);
    row->addWidget(m_steps);
    row->addSpacing(kSpacing);
    row->addWidget(m_stop);
    row->addWidget(m_discard);
    setSteps(0);

    connect(m_stop, &QAbstractButton::clicked, this, &RecordingBar::stopRequested);
    connect(m_discard, &QAbstractButton::clicked, this, &RecordingBar::cancelRequested);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void RecordingBar::setElapsed(qint64 milliseconds)
{
    m_time->setText(clockText(milliseconds));
}

void RecordingBar::setSteps(int steps)
{
    m_steps->setText(steps == 1 ? QStringLiteral("1 step") : QStringLiteral("%1 steps").arg(steps));
}

void RecordingBar::placeNear(QWidget *window)
{
    adjustSize();
    QScreen *screen = window ? window->screen() : nullptr;
    const QRect area = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
    move(area.center().x() - width() / 2, area.top() + kTopGap);
}

void RecordingBar::paintEvent(QPaintEvent *)
{
    const SettingsPalette &palette = currentTheme().settings;
    QPainter painter(this);
    preparePainter(painter);
    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(palette.cardBorder, 1));
    painter.setBrush(palette.windowFill);
    painter.drawRoundedRect(body, kRadius, kRadius);
}

void RecordingBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - pos();
    }
}

void RecordingBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        move(event->globalPosition().toPoint() - m_dragOffset);
    }
}

void RecordingBar::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
}

}  // namespace sky
