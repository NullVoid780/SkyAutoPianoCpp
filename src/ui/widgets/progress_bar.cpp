#include "ui/widgets/progress_bar.hpp"

#include <QPainter>
#include <QVariantAnimation>

#include <algorithm>

#include "ui/core/easing.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kHeight = 6;

}  // namespace

ProgressBar::ProgressBar(Palette palette, QWidget *parent)
    : QWidget(parent), m_palette(std::move(palette)), m_animation(new QVariantAnimation(this))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_animation->setDuration(kHoverSlowMs);
    m_animation->setEasingCurve(easingCurve(Easing::Ease));
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_shown = value.toReal();
        update();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void ProgressBar::setValue(int percent)
{
    percent = std::clamp(percent, 0, 100);
    if (percent == m_target) {
        return;
    }
    m_target = percent;
    m_animation->stop();
    m_animation->setStartValue(m_shown);
    m_animation->setEndValue(static_cast<qreal>(percent));
    m_animation->start();
}

QSize ProgressBar::sizeHint() const
{
    return {200, kHeight};
}

void ProgressBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    preparePainter(painter);
    const QRectF track(0, (height() - kHeight) / 2.0, width(), kHeight);
    paintPill(painter, track, m_palette.track());
    if (m_shown <= 0) {
        return;
    }
    const QRectF fill(track.left(), track.top(), std::max<qreal>(kHeight, track.width() * m_shown / 100.0), kHeight);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_palette.fillStart());
    painter.drawRoundedRect(fill, kHeight / 2.0, kHeight / 2.0);
}

}  // namespace sky
