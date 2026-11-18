#include "ui/core/hover_tracker.hpp"

#include <QEvent>
#include <QWidget>

namespace sky {

HoverTracker::HoverTracker(QWidget *target, int durationMs, Easing easing)
    : QObject(target), m_target(target), m_animation(new QVariantAnimation(this))
{
    m_animation->setDuration(durationMs);
    m_animation->setEasingCurve(easingCurve(easing));
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_progress = value.toReal();
        m_target->update();
        emit changed(m_progress);
    });
    target->setAttribute(Qt::WA_Hover);
    target->installEventFilter(this);
}

void HoverTracker::setActive(bool active)
{
    animateTo(active ? 1.0 : 0.0);
}

void HoverTracker::animateTo(qreal target)
{
    m_animation->stop();
    m_animation->setStartValue(m_progress);
    m_animation->setEndValue(target);
    m_animation->start();
}

bool HoverTracker::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_target) {
        if (event->type() == QEvent::Enter || event->type() == QEvent::HoverEnter) {
            if (!m_hovered) {
                m_hovered = true;
                animateTo(1.0);
            }
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave) {
            if (m_hovered) {
                m_hovered = false;
                animateTo(0.0);
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

}  // namespace sky
