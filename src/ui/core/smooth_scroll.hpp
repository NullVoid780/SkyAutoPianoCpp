#pragma once

#include <QObject>

class QAbstractScrollArea;
class QPropertyAnimation;

namespace sky {

class SmoothScroll final : public QObject {
    Q_OBJECT

public:
    explicit SmoothScroll(QAbstractScrollArea *area);

    void scrollTo(int target, int durationMs);
    void stop();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QAbstractScrollArea *m_area;
    QPropertyAnimation *m_animation;
    int m_target = 0;
};

}  // namespace sky
