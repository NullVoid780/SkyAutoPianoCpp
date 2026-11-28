#pragma once

#include <functional>

#include "ui/core/overlay.hpp"
#include "ui/widgets/action_button.hpp"

namespace sky {

class IconButton;

class ConfirmDialog final : public Overlay {
    Q_OBJECT

public:
    enum class Choice { Dismissed, Secondary, Primary };

    struct Content {
        QString iconId;
        QString title;
        QString body;
        QString secondaryLabel;
        QString secondaryIconId;
        QString primaryLabel;
        QString primaryIconId;
        ActionButton::Kind primaryKind = ActionButton::Kind::Primary;
    };

    ConfirmDialog(QWidget *window, Content content);

    void ask(std::function<void(Choice choice)> done);

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
    void finish(Choice choice);

    QString m_iconId;
    QWidget *m_panel;
    QWidget *m_iconSpace;
    ActionButton *m_secondary;
    ActionButton *m_primary;
    IconButton *m_close;
    std::function<void(Choice)> m_done;
    Choice m_choice = Choice::Dismissed;
};

}  // namespace sky
