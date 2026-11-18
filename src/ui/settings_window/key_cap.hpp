#pragma once

#include <QWidget>

namespace sky {

class HoverTracker;

class KeyCap final : public QWidget {
    Q_OBJECT

public:
    enum Shape { Square = 1, Circle = 2 };

    KeyCap(int shapes, QWidget *parent = nullptr);

    void setValue(const QString &value);
    const QString &value() const { return m_value; }
    QSize sizeHint() const override;

signals:
    void edited();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    int m_shapes;
    QString m_value;
    QString m_label;
    HoverTracker *m_hover;
};

}  // namespace sky
