#include "ui/core/smooth_scroll.hpp"

#include <QAbstractScrollArea>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>

namespace sky {

namespace {

constexpr int kWheelStepPixels = 110;
constexpr int kWheelAnimationMs = 220;

}  // namespace

SmoothScroll::SmoothScroll(QAbstractScrollArea *area)
    : QObject(area), m_area(area), m_animation(new QPropertyAnimation(area->verticalScrollBar(), "value", this))
{
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    area->viewport()->installEventFilter(this);
    connect(area->verticalScrollBar(), &QScrollBar::sliderPressed, m_animation, &QPropertyAnimation::stop);
}

void SmoothScroll::scrollTo(int target, int durationMs)
{
    QScrollBar *bar = m_area->verticalScrollBar();
    m_target = std::clamp(target, bar->minimum(), bar->maximum());
    m_animation->stop();
    m_animation->setDuration(durationMs);
    m_animation->setStartValue(bar->value());
    m_animation->setEndValue(m_target);
    m_animation->start();
}

void SmoothScroll::stop()
{
    m_animation->stop();
}

bool SmoothScroll::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_area->viewport() || event->type() != QEvent::Wheel) {
        return QObject::eventFilter(watched, event);
    }
    auto *wheel = static_cast<QWheelEvent *>(event);
    const int notches = wheel->angleDelta().y();
    if (!wheel->pixelDelta().isNull() || notches == 0 || (wheel->modifiers() & Qt::ControlModifier)) {
        m_animation->stop();
        return false;
    }
    const int from = m_animation->state() == QAbstractAnimation::Running ? m_target : m_area->verticalScrollBar()->value();
    scrollTo(from - notches * kWheelStepPixels / 120, kWheelAnimationMs);
    wheel->accept();
    return true;
}

}  // namespace sky
