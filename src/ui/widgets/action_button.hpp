#pragma once

#include <functional>
#include <vector>

#include <QAbstractButton>
#include <QColor>

#include "ui/theme.hpp"

class QTimer;

namespace sky {

class CardFrame;
class ElevationLayer;
class HoverTracker;

struct ActionButtonStyle {
    std::function<QColor()> fillStart;
    std::function<QColor()> fillEnd;
    std::function<QColor()> hoverFill;
    std::function<QColor()> border;
    std::function<QColor()> text;
    std::function<ShadowSpec()> shadow;
    qreal radius = 12;
    qreal borderWidth = 1;
    int minHeight = 36;
    int minWidth = 0;
    int padX = 12;
    int iconSize = 13;
    int gap = 10;
    int fontSize = 13;
    qreal lift = 2;
    qreal iconSpin = 0;
};

class ActionButton final : public QAbstractButton {
    Q_OBJECT

public:
    enum class Kind { Primary, Secondary, Check, Cancel, Confirm, UpdateNow, Later, Subtle, Amber };

    ActionButton(Kind kind, const QString &iconId, const QString &label, QWidget *parent = nullptr);

    void attachShadows(ElevationLayer *layer);
    void attachShadows(CardFrame *card);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

    static ActionButtonStyle styleFor(Kind kind);
    void setBusy(bool busy, const QString &busyLabel = QString());
    void setIconId(const QString &iconId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    qreal liftAmount() const;
    QRectF bodyRect() const;
    QFont labelFont() const;

    ActionButtonStyle m_style;
    QString m_iconId;
    HoverTracker *m_hover;
    ElevationLayer *m_layer = nullptr;
    bool m_keyboardFocus = false;
    bool m_busy = false;
    QString m_idleLabel;
    qreal m_spin = 0;
    QTimer *m_spinner;
};

}  // namespace sky
