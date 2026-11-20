#include "ui/practice/play_along_overlay.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

#include "ui/practice/sky_guide_stage.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/settings_window/action_button.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/toggle_switch.hpp"

namespace sky {

namespace {

constexpr int kBarPadding = 8;
constexpr qreal kBarRadius = 14;
constexpr int kTitleWidth = 240;
constexpr int kEdge = 12;
constexpr int kMinWidth = 260;
constexpr int kMinHeight = 160;
constexpr int kLeft = 1;
constexpr int kRight = 2;
constexpr int kTop = 4;
constexpr int kBottom = 8;

}  // namespace

PlayAlongOverlay::PlayAlongOverlay() : QWidget(nullptr, flagsFor(false)), m_stage(new SkyGuideStage(this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_NoSystemBackground);
    setWindowTitle(QStringLiteral("Sky Auto Piano play-along"));
}

Qt::WindowFlags PlayAlongOverlay::flagsFor(bool moving) const
{
    Qt::WindowFlags flags = Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus |
                            Qt::NoDropShadowWindowHint;
    if (!moving) {
        flags |= Qt::WindowTransparentForInput;
    }
    return flags;
}

void PlayAlongOverlay::setMoveMode(bool moving)
{
    if (moving == m_moving) {
        return;
    }
    m_moving = moving;
    const bool visible = isVisible();
    setWindowFlags(flagsFor(moving));
    m_stage->setAttribute(Qt::WA_TransparentForMouseEvents, moving);
    m_stage->setMoveMode(moving);
    setMouseTracking(moving);
    setCursor(moving ? Qt::SizeAllCursor : Qt::ArrowCursor);
    if (visible) {
        show();
    }
}

void PlayAlongOverlay::resizeEvent(QResizeEvent *)
{
    m_stage->setGeometry(rect());
}

int PlayAlongOverlay::edgesAt(const QPoint &point) const
{
    if (!m_floating) {
        return 0;
    }
    int edges = 0;
    if (point.x() < kEdge) {
        edges |= kLeft;
    } else if (point.x() > width() - kEdge) {
        edges |= kRight;
    }
    if (point.y() < kEdge) {
        edges |= kTop;
    } else if (point.y() > height() - kEdge) {
        edges |= kBottom;
    }
    return edges;
}

void PlayAlongOverlay::updateCursor(int edges)
{
    if (!m_moving) {
        setCursor(Qt::ArrowCursor);
    } else if ((edges & kLeft && edges & kTop) || (edges & kRight && edges & kBottom)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if ((edges & kRight && edges & kTop) || (edges & kLeft && edges & kBottom)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (edges & (kLeft | kRight)) {
        setCursor(Qt::SizeHorCursor);
    } else if (edges & (kTop | kBottom)) {
        setCursor(Qt::SizeVerCursor);
    } else {
        setCursor(Qt::SizeAllCursor);
    }
}

void PlayAlongOverlay::mousePressEvent(QMouseEvent *event)
{
    if (m_moving && event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_last = event->globalPosition().toPoint();
        m_startPoint = m_last;
        m_startGeometry = geometry();
        m_edges = edgesAt(event->position().toPoint());
    }
}

void PlayAlongOverlay::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint current = event->globalPosition().toPoint();
    if (!m_dragging) {
        updateCursor(edgesAt(event->position().toPoint()));
        return;
    }
    if (!m_floating) {
        emit dragged(current - m_last);
        m_last = current;
        return;
    }
    const QPoint delta = current - m_startPoint;
    QRect next = m_startGeometry;
    if (m_edges == 0) {
        next.moveTopLeft(m_startGeometry.topLeft() + delta);
    } else {
        if (m_edges & kLeft) {
            next.setLeft(std::min(m_startGeometry.left() + delta.x(), m_startGeometry.right() - kMinWidth));
        }
        if (m_edges & kRight) {
            next.setRight(std::max(m_startGeometry.right() + delta.x(), m_startGeometry.left() + kMinWidth));
        }
        if (m_edges & kTop) {
            next.setTop(std::min(m_startGeometry.top() + delta.y(), m_startGeometry.bottom() - kMinHeight));
        }
        if (m_edges & kBottom) {
            next.setBottom(std::max(m_startGeometry.bottom() + delta.y(), m_startGeometry.top() + kMinHeight));
        }
    }
    emit windowEdited(next);
}

void PlayAlongOverlay::mouseReleaseEvent(QMouseEvent *)
{
    if (m_dragging) {
        m_dragging = false;
        emit windowEditFinished();
    }
}

void PlayAlongOverlay::wheelEvent(QWheelEvent *event)
{
    if (m_moving && event->angleDelta().y() != 0) {
        emit wheelResized(event->angleDelta().y() > 0 ? 1 : -1);
    }
}

AlignPrompt::AlignPrompt()
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus |
                           Qt::WindowTransparentForInput | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowTitle(QStringLiteral("Sky Auto Piano auto-align"));
    setFixedSize(440, 132);
}

void AlignPrompt::showStep(int step, const QString &key, const QString &note)
{
    m_resultMode = false;
    m_step = step;
    m_key = key;
    m_note = note;
    update();
}

void AlignPrompt::showResult(bool ok, const QString &message)
{
    m_resultMode = true;
    m_ok = ok;
    m_result = message;
    update();
}

void AlignPrompt::paintEvent(QPaintEvent *)
{
    const QColor ink(255, 255, 255);
    QPainter painter(this);
    preparePainter(painter);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    const QRectF body = QRectF(rect()).adjusted(1, 1, -1, -1);
    painter.setPen(QPen(QColor(255, 255, 255, 80), 1));
    painter.setBrush(QColor(24, 15, 12, 215));
    painter.drawRoundedRect(body, 18, 18);

    if (m_resultMode) {
        const QPointF mark(body.left() + 52, body.center().y());
        const QColor tone = m_ok ? QColor(120, 220, 150) : QColor(255, 150, 120);
        painter.setPen(QPen(tone, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(mark, 18, 18);
        if (m_ok) {
            painter.drawPolyline(QPolygonF({mark + QPointF(-8, 0), mark + QPointF(-2, 7), mark + QPointF(9, -7)}));
        } else {
            painter.drawLine(mark + QPointF(0, -9), mark + QPointF(0, 3));
            painter.drawPoint(mark + QPointF(0, 9));
        }
        painter.setFont(FontLoader::font(16, QFont::DemiBold));
        painter.setPen(ink);
        painter.drawText(body.adjusted(88, 0, -20, 0), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap, m_result);
        return;
    }

    painter.setFont(FontLoader::font(11, QFont::Bold));
    painter.setPen(QColor(255, 255, 255, 150));
    painter.drawText(QRectF(body.left(), body.top() + 12, body.width(), 18), Qt::AlignCenter,
                     QStringLiteral("AUTO-ALIGN  \u00b7  STEP %1 OF 2").arg(m_step));

    const QFont words = FontLoader::font(22, QFont::DemiBold);
    const QFontMetrics metrics(words);
    const QString before = QStringLiteral("Press");
    const QString after = QStringLiteral("in Sky");
    const qreal cap = 46;
    const qreal gap = 14;
    const qreal total = metrics.horizontalAdvance(before) + gap + cap + gap + metrics.horizontalAdvance(after);
    qreal x = body.center().x() - total / 2;
    const qreal middle = body.top() + 62;
    painter.setFont(words);
    painter.setPen(ink);
    painter.drawText(QRectF(x, middle - 20, metrics.horizontalAdvance(before) + 2, 40), Qt::AlignVCenter | Qt::AlignLeft, before);
    x += metrics.horizontalAdvance(before) + gap;
    const QRectF key(x, middle - cap / 2, cap, cap);
    for (int ring = 3; ring >= 1; --ring) {
        painter.setPen(QPen(QColor(255, 255, 255, 22 * ring), ring * 2.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(key.adjusted(-ring, -ring, ring, ring), 9 + ring, 9 + ring);
    }
    painter.setPen(QPen(ink, 2.4));
    painter.setBrush(QColor(24, 15, 12, 160));
    painter.drawRoundedRect(key, 9, 9);
    painter.setFont(FontLoader::font(22, QFont::Bold));
    painter.drawText(key, Qt::AlignCenter, m_key);
    x += cap + gap;
    painter.setFont(words);
    painter.drawText(QRectF(x, middle - 20, metrics.horizontalAdvance(after) + 2, 40), Qt::AlignVCenter | Qt::AlignLeft, after);

    painter.setFont(FontLoader::font(12, QFont::Normal));
    painter.setPen(QColor(255, 255, 255, 185));
    painter.drawText(QRectF(body.left() + 16, body.bottom() - 34, body.width() - 32, 22), Qt::AlignCenter, m_note);
}

PlayAlongBar::PlayAlongBar()
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus |
                           Qt::NoDropShadowWindowHint),
      m_title(new QLabel(this)),
      m_wait(new ToggleSwitch(this)),
      m_speed(new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("speedometer2"), QString(), this)),
      m_opacity(new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("eye"), QString(), this)),
      m_move(new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("arrow-up"), QStringLiteral("Move"), this)),
      m_floating(new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("window"), QStringLiteral("On Sky"), this)),
      m_align(new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("lightning"), QStringLiteral("Auto-align"), this)),
      m_restart(new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("arrow-clockwise"), QStringLiteral("Restart"),
                                 this)),
      m_stop(new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("x-lg"), QStringLiteral("Stop"), this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowTitle(QStringLiteral("Sky Auto Piano play-along controls"));
    m_title->setObjectName(QStringLiteral("PracticeLabel"));
    m_title->setFixedWidth(kTitleWidth);
    auto *waitLabel = new QLabel(QStringLiteral("Wait for me"), this);
    waitLabel->setObjectName(QStringLiteral("PracticeLabel"));
    m_wait->setToolTip(QStringLiteral("The guide stops on each note until you press it in Sky."));
    m_speed->setToolTip(QStringLiteral("Change the speed"));
    m_opacity->setToolTip(QStringLiteral("Make the guide more or less see-through"));
    m_move->setToolTip(QStringLiteral("Drag the guide to a new place and scroll to resize it"));
    m_align->setToolTip(QStringLiteral("Line the guide up with Sky's own keys. Open Sky's instrument first."));
    m_floating->setToolTip(QStringLiteral("On Sky: the guide sits on the game. Floating: the guide is its own window you can "
                                          "move and resize."));
    for (QWidget *control : std::initializer_list<QWidget *>{m_wait, m_speed, m_opacity, m_move, m_floating, m_align, m_restart, m_stop}) {
        control->setFocusPolicy(Qt::NoFocus);
    }

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(kBarPadding + 6, kBarPadding, kBarPadding, kBarPadding);
    row->setSpacing(8);
    row->addWidget(m_title);
    row->addWidget(waitLabel);
    row->addWidget(m_wait);
    row->addSpacing(4);
    row->addWidget(m_speed);
    row->addWidget(m_opacity);
    row->addWidget(m_floating);
    row->addWidget(m_align);
    row->addWidget(m_move);
    row->addWidget(m_restart);
    row->addWidget(m_stop);

    connect(m_wait, &QAbstractButton::toggled, this, &PlayAlongBar::waitToggled);
    connect(m_speed, &QAbstractButton::clicked, this, &PlayAlongBar::speedClicked);
    connect(m_opacity, &QAbstractButton::clicked, this, &PlayAlongBar::opacityClicked);
    connect(m_move, &QAbstractButton::clicked, this, &PlayAlongBar::moveClicked);
    connect(m_floating, &QAbstractButton::clicked, this, &PlayAlongBar::floatingClicked);
    connect(m_align, &QAbstractButton::clicked, this, &PlayAlongBar::alignClicked);
    connect(m_restart, &QAbstractButton::clicked, this, &PlayAlongBar::restartClicked);
    connect(m_stop, &QAbstractButton::clicked, this, &PlayAlongBar::stopClicked);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void PlayAlongBar::setTitle(const QString &title)
{
    m_titleText = title;
    m_title->setText(m_title->fontMetrics().elidedText(title, Qt::ElideRight, kTitleWidth));
    m_title->setToolTip(title);
}

void PlayAlongBar::setMessage(const QString &message)
{
    const QString text = message.isEmpty() ? m_titleText : message;
    m_title->setText(m_title->fontMetrics().elidedText(text, Qt::ElideRight, kTitleWidth));
    m_title->setToolTip(text);
}

void PlayAlongBar::setAligning(bool aligning)
{
    m_align->setText(aligning ? QStringLiteral("Cancel") : QStringLiteral("Auto-align"));
    m_align->setIconId(aligning ? QStringLiteral("x-lg") : QStringLiteral("lightning"));
    adjustSize();
}

void PlayAlongBar::setWait(bool wait)
{
    m_wait->setCheckedImmediately(wait);
}

void PlayAlongBar::setSpeed(double speed)
{
    m_speed->setText(QStringLiteral("%1%").arg(qRound(speed * 100)));
    adjustSize();
}

void PlayAlongBar::setOpacityPercent(int percent)
{
    m_opacity->setText(QStringLiteral("%1%").arg(percent));
    adjustSize();
}

void PlayAlongBar::setMoving(bool moving)
{
    m_move->setText(moving ? QStringLiteral("Lock") : QStringLiteral("Move"));
    m_move->setIconId(moving ? QStringLiteral("check-lg") : QStringLiteral("arrow-up"));
    adjustSize();
}

void PlayAlongBar::setFloating(bool floating)
{
    m_floating->setText(floating ? QStringLiteral("Floating") : QStringLiteral("On Sky"));
    m_move->setToolTip(floating ? QStringLiteral("Drag the window to move it, drag its edges to resize it")
                                : QStringLiteral("Drag the guide to line it up with Sky's keys, scroll to resize it"));
    adjustSize();
}

void PlayAlongBar::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(theme.borderColor, 1));
    painter.setBrush(withAlpha(theme.background, 0.94));
    painter.drawRoundedRect(body, kBarRadius, kBarRadius);
}

}  // namespace sky
