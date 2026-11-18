#pragma once

#include <QAbstractButton>

namespace sky {

class HoverTracker;

class SaveButton final : public QAbstractButton {
    Q_OBJECT

public:
    static constexpr int kDiameter = 60;
    static constexpr int kLift = 2;

    explicit SaveButton(QWidget *parent = nullptr);

    QRectF bodyRect() const;

signals:
    void bodyMoved();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool hitButton(const QPoint &pos) const override;

private:
    HoverTracker *m_hover;
    bool m_keyboardFocus = false;
};

}  // namespace sky
