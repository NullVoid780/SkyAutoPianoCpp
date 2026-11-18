#pragma once

#include <functional>

#include <QAbstractButton>
#include <QColor>
#include <QString>

#include "ui/core/easing.hpp"

namespace sky {

class HoverTracker;

struct IconButtonStyle {
    std::function<QColor()> idle;
    std::function<QColor()> hover;
    std::function<QColor()> frameFill;
    std::function<QColor()> frameBorder;
    qreal frameRadius = 0;
    qreal hoverRotation = 0;
    int durationMs = kHoverSlowMs;
};

class IconButton final : public QAbstractButton {
    Q_OBJECT

public:
    IconButton(const QString &iconId, int iconSize, QSize boxSize, IconButtonStyle style, QWidget *parent = nullptr);

    void setIconId(const QString &iconId);
    const QString &iconId() const { return m_iconId; }
    QSize sizeHint() const override { return m_boxSize; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    bool m_keyboardFocus = false;
    QString m_iconId;
    int m_iconSize;
    QSize m_boxSize;
    IconButtonStyle m_style;
    HoverTracker *m_hover;
};

}  // namespace sky
