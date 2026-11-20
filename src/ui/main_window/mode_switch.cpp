#include "ui/main_window/mode_switch.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QVariantAnimation>

#include <utility>

#include "ui/core/easing.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kHeight = 24;
constexpr int kPadX = 12;
constexpr int kInset = 2;
constexpr int kFontSize = 12;
constexpr int kSlideMs = 200;

}  // namespace

ModeSwitch::ModeSwitch(QStringList labels, QWidget *parent)
    : QWidget(parent), m_labels(std::move(labels)), m_animation(new QVariantAnimation(this))
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    m_animation->setDuration(kSlideMs);
    m_animation->setEasingCurve(easingCurve(Easing::Ease));
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_slide = value.toReal();
        update();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

qreal ModeSwitch::segmentWidth(int index) const
{
    const QFontMetrics metrics(FontLoader::font(kFontSize, QFont::DemiBold));
    return metrics.horizontalAdvance(m_labels.value(index)) + kPadX * 2;
}

QSize ModeSwitch::sizeHint() const
{
    qreal width = kInset * 2;
    for (int i = 0; i < m_labels.size(); ++i) {
        width += segmentWidth(i);
    }
    return QSize(static_cast<int>(width), kHeight);
}

QRectF ModeSwitch::segment(int index) const
{
    qreal left = kInset;
    for (int i = 0; i < index; ++i) {
        left += segmentWidth(i);
    }
    return QRectF(left, kInset, segmentWidth(index), height() - kInset * 2);
}

void ModeSwitch::setCurrent(int index)
{
    if (index < 0 || index >= m_labels.size() || index == m_current) {
        return;
    }
    m_current = index;
    m_animation->stop();
    m_animation->setStartValue(m_slide);
    m_animation->setEndValue(static_cast<qreal>(index));
    m_animation->start();
}

void ModeSwitch::mousePressEvent(QMouseEvent *event)
{
    for (int i = 0; i < m_labels.size(); ++i) {
        if (segment(i).contains(event->position()) && i != m_current) {
            setCurrent(i);
            emit chosen(i);
            return;
        }
    }
}

void ModeSwitch::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right || event->key() == Qt::Key_Space) {
        const int next = (m_current + 1) % static_cast<int>(m_labels.size());
        setCurrent(next);
        emit chosen(next);
        return;
    }
    QWidget::keyPressEvent(event);
}

void ModeSwitch::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(withAlpha(theme.footerText, hasFocus() ? 0.6 : 0.25), 1));
    painter.setBrush(withAlpha(theme.footerText, 0.06));
    painter.drawRoundedRect(body, body.height() / 2, body.height() / 2);

    const int lower = static_cast<int>(m_slide);
    const int upper = std::min(lower + 1, static_cast<int>(m_labels.size()) - 1);
    const qreal fraction = m_slide - lower;
    const QRectF from = segment(lower);
    const QRectF to = segment(upper);
    const QRectF thumb(from.left() + (to.left() - from.left()) * fraction, from.top(),
                       from.width() + (to.width() - from.width()) * fraction, from.height());
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme.hoverColor);
    painter.drawRoundedRect(thumb, thumb.height() / 2, thumb.height() / 2);

    painter.setFont(FontLoader::font(kFontSize, QFont::DemiBold));
    for (int i = 0; i < m_labels.size(); ++i) {
        const qreal closeness = std::max(0.0, 1.0 - std::abs(m_slide - i));
        painter.setPen(blend(theme.footerText, theme.accent.white, closeness));
        painter.drawText(segment(i), Qt::AlignCenter, m_labels.at(i));
    }
}

}  // namespace sky
