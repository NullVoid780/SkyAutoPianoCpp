#pragma once

#include <functional>

#include "ui/core/overlay.hpp"

namespace sky {

class ActionButton;
class IconButton;

class BlockUpdatesDialog final : public Overlay {
    Q_OBJECT

public:
    explicit BlockUpdatesDialog(QWidget *window);

    void ask(std::function<void(bool confirmed)> done);

protected:
    QWidget *panel() const override { return m_panel; }
    QColor dimColor() const override;
    qreal backdropBlurRadius() const override { return 5; }
    ShadowSpec panelShadow() const override;
    qreal panelRadius() const override { return 20; }
    QRect panelGeometry() const override;
    void paintPanel(QPainter &painter, const QRectF &rect) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void finish(bool confirmed);

    QWidget *m_panel;
    QWidget *m_iconSpace;
    ActionButton *m_cancel;
    ActionButton *m_confirm;
    IconButton *m_close;
    std::function<void(bool)> m_done;
    bool m_confirmed = false;
};

}  // namespace sky
