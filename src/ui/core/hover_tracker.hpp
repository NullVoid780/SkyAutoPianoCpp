#pragma once

#include <QObject>
#include <QVariantAnimation>

#include "ui/core/easing.hpp"

class QWidget;

namespace sky {

class HoverTracker final : public QObject {
    Q_OBJECT

public:
    HoverTracker(QWidget *target, int durationMs, Easing easing = Easing::Linear);

    qreal progress() const { return m_progress; }
    bool isHovered() const { return m_hovered; }
    void setActive(bool active);

signals:
    void changed(qreal progress);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void animateTo(qreal target);

    QWidget *m_target;
    QVariantAnimation *m_animation;
    qreal m_progress = 0;
    bool m_hovered = false;
};

}  // namespace sky
