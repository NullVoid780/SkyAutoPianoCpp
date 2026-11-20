#pragma once

#include <QStringList>
#include <QWidget>

class QVariantAnimation;

namespace sky {

class ModeSwitch final : public QWidget {
    Q_OBJECT

public:
    ModeSwitch(QStringList labels, QWidget *parent = nullptr);

    int current() const { return m_current; }
    void setCurrent(int index);
    QSize sizeHint() const override;

signals:
    void chosen(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QRectF segment(int index) const;
    qreal segmentWidth(int index) const;

    QStringList m_labels;
    int m_current = 0;
    qreal m_slide = 0;
    QVariantAnimation *m_animation;
};

}  // namespace sky
