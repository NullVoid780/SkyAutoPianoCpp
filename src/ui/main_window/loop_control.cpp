#include "ui/main_window/loop_control.hpp"

#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include "ui/core/hover_tracker.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kGlyph = 16;
constexpr qreal kOffOpacity = 0.45;

QString captionText(int seconds)
{
    return QStringLiteral("Delay next: %1s").arg(seconds);
}

}  // namespace

LoopButton::LoopButton(QWidget *parent) : QWidget(parent), m_hover(new HoverTracker(this, kHoverFastMs))
{
    setFixedSize(sizeHint());
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
}

void LoopButton::setMode(LoopMode mode)
{
    m_mode = mode;
    update();
}

void LoopButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
        emit clicked();
    }
}

void LoopButton::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit clicked();
        return;
    }
    QWidget::keyPressEvent(event);
}

void LoopButton::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    const QRectF glyph((width() - kGlyph) / 2.0, (height() - kGlyph) / 2.0, kGlyph, kGlyph);
    const QColor active = blend(theme.loopIcon, theme.loopIconHover, m_hover->progress());
    const QColor color = m_mode == LoopMode::Off ? withAlpha(active, kOffOpacity + (1.0 - kOffOpacity) * m_hover->progress())
                                                 : active;
    const QString icon = m_mode == LoopMode::One ? QStringLiteral("loop-one") : QStringLiteral("loop");
    painter.drawPixmap(glyph.topLeft(), IconCache::pixmap(icon, kGlyph, color, devicePixelRatioF()));
    if (hasFocus()) {
        painter.setPen(QPen(withAlpha(color, 0.6), 1, Qt::DotLine));
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
    }
}

LoopControl::LoopControl(QWidget *parent)
    : QWidget(parent), m_button(new LoopButton(this)), m_slider(new QSlider(Qt::Horizontal, this)), m_caption(new QLabel(this))
{
    m_slider->setObjectName(QStringLiteral("LoopSlider"));
    m_slider->setRange(0, 5);
    m_slider->setSingleStep(1);
    m_slider->setPageStep(1);
    m_caption->setObjectName(QStringLiteral("DelayCaption"));
    m_caption->setText(captionText(1));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    m_slider->setFixedHeight(8);
    layout->addWidget(m_button, 0, Qt::AlignHCenter);
    layout->addWidget(m_slider, 0, Qt::AlignHCenter);
    layout->addWidget(m_caption, 0, Qt::AlignHCenter);

    connect(m_button, &LoopButton::clicked, this, &LoopControl::cycleRequested);
    connect(m_slider, &QSlider::valueChanged, this, [this](int value) {
        m_caption->setText(captionText(value));
        emit delayChanged(value);
    });
}

void LoopControl::setMode(LoopMode mode)
{
    m_button->setMode(mode);
}

void LoopControl::setDelaySeconds(int seconds)
{
    const QSignalBlocker blocker(m_slider);
    m_slider->setValue(seconds);
    m_caption->setText(captionText(seconds));
}

}  // namespace sky
