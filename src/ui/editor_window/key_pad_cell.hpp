#pragma once

#include <QAbstractButton>
#include <QColor>

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
    void setKeyIndex(int index);
    void flash();
    void setCellSize(int size);
    QSize sizeHint() const override { return QSize(m_size, m_size); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_key;
    int m_shapes;
    int m_size = kSize;
    bool m_active = false;
    int m_keyIndex = -1;
    bool m_flashing = false;
    HoverTracker *m_hover;
    QTimer *m_flash;
};

}  // namespace sky
