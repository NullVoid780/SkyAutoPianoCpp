#pragma once

#include <QAbstractButton>

class QVariantAnimation;

namespace sky {

class ToggleSwitch final : public QAbstractButton {
    Q_OBJECT

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    void setCheckedImmediately(bool checked);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool hitButton(const QPoint &pos) const override;

private:
    QRectF trackRect() const;

    QVariantAnimation *m_slide;
    qreal m_position = 0;
    bool m_keyboardFocus = false;
};

}  // namespace sky
