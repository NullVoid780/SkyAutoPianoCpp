#pragma once

#include <QAbstractButton>

namespace sky {

class HoverTracker;

class SidebarItem final : public QAbstractButton {
    Q_OBJECT

public:
    static constexpr qreal kRadius = 12;

    SidebarItem(const QString &iconId, const QString &label, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    void setBadge(bool visible);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    QString m_iconId;
    HoverTracker *m_hover;
    bool m_keyboardFocus = false;
    bool m_badge = false;
};

}  // namespace sky
