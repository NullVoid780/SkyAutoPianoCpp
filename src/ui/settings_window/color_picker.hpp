#pragma once

#include <functional>
#include <optional>

#include <QColor>

#include "ui/core/overlay.hpp"

class QLabel;
class QLineEdit;

namespace sky {

class ActionButton;
class ColorCompare;
class ColorStrip;
class SaturationValueField;

void paintColorSwatch(QPainter &painter, const QRectF &rect, const QColor &color, qreal radius);

class ColorPicker final : public Overlay {
    Q_OBJECT

public:
    explicit ColorPicker(QWidget *window);

    void pick(const QString &title, const QColor &initial, std::function<void(const QColor &)> live,
              std::function<void(std::optional<QColor>)> done);

protected:
    QWidget *panel() const override { return m_panel; }
    QColor dimColor() const override;
    ShadowSpec panelShadow() const override;
    qreal panelRadius() const override { return 20; }
    QRect panelGeometry() const override;
    void paintPanel(QPainter &painter, const QRectF &rect) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setColor(const QColor &color, bool fromText);
    void finish(bool applied);

    QWidget *m_panel;
    QLabel *m_title;
    SaturationValueField *m_field;
    ColorStrip *m_hue;
    ColorStrip *m_alpha;
    ColorCompare *m_compare;
    QLineEdit *m_hex;
    ActionButton *m_cancel;
    ActionButton *m_apply;
    QColor m_original;
    QColor m_color;
    qreal m_hueValue = 0;
    bool m_applied = false;
    std::function<void(const QColor &)> m_live;
    std::function<void(std::optional<QColor>)> m_done;
};

}  // namespace sky
