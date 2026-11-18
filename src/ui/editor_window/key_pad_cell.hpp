#pragma once

#include <QAbstractButton>

class QTimer;

namespace sky {

class HoverTracker;

class KeyPadCell final : public QAbstractButton {
    Q_OBJECT

public:
    enum Shape { Square = 1, Circle = 2 };

    static constexpr int kSize = 59;

    KeyPadCell(const QString &key, int shapes, QWidget *parent = nullptr);

    const QString &key() const { return m_key; }
    void setActive(bool active);
    void flash();
    QSize sizeHint() const override { return QSize(kSize, kSize); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_key;
    int m_shapes;
    bool m_active = false;
    bool m_flashing = false;
    HoverTracker *m_hover;
    QTimer *m_flash;
};

}  // namespace sky
