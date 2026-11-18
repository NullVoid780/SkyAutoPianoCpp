#pragma once

#include <functional>

#include "ui/core/overlay.hpp"

class QLabel;
class QPushButton;

namespace sky {

class ConfirmPopup final : public Overlay {
    Q_OBJECT

public:
    explicit ConfirmPopup(QWidget *window);

    void ask(const QString &message, std::function<void()> onConfirm);

protected:
    QWidget *panel() const override;
    QColor dimColor() const override;
    qreal backdropBlurRadius() const override { return 25; }
    ShadowSpec panelShadow() const override;
    qreal panelRadius() const override;
    QRect panelGeometry() const override;
    void paintPanel(QPainter &painter, const QRectF &rect) override;

private:
    QWidget *m_panel;
    QLabel *m_message;
    QPushButton *m_confirm;
    QPushButton *m_cancel;
    std::function<void()> m_onConfirm;
};

}  // namespace sky
