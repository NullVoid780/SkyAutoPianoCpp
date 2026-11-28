#pragma once

#include <QElapsedTimer>
#include <QWidget>

#include "model/toast.hpp"
#include "ui/render/backdrop_blur.hpp"

class QTimer;
class QVariantAnimation;

namespace sky {

class ToastItem final : public QWidget {
    Q_OBJECT

public:
    static constexpr int kWidth = 320;

    ToastItem(const Toast &toast, QWidget *parent);

    qreal presence() const { return m_presence; }
    int preferredHeight() const;
    void enter();
    void dismiss();
    bool isLeaving() const { return m_leaving; }

signals:
    void presenceChanged();
    void finished(sky::ToastItem *item);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QRectF closeRect() const;
    QRectF textRect(int height) const;
    QString displayText() const;

    Toast m_toast;
    QVariantAnimation *m_animation;
    QTimer *m_ticker;
    QTimer *m_backdropRefresh;
    BackdropBlur m_backdrop;
    QElapsedTimer m_clock;
    qreal m_presence = 0;
    bool m_leaving = false;
    bool m_closeHovered = false;
    mutable int m_cachedHeight = 0;
};

}  // namespace sky
