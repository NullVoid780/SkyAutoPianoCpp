#include "ui/settings_window/settings_panel.hpp"

#include <QPainter>

#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

SettingsPanel::SettingsPanel(int padding, QWidget *parent) : CardFrame(0, 0, false, parent)
{
    setPadding(QMargins(padding, padding, padding, padding));
}

void SettingsPanel::paintCard(QPainter &painter, const QRectF &body, qreal)
{
    const SettingsPalette &s = currentTheme().settings;
    const QPainterPath path = roundedPath(body.adjusted(0.5, 0.5, -0.5, -0.5), cornerRadius());
    painter.fillPath(path, s.cardFill);
    painter.setPen(QPen(s.cardBorder, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

}  // namespace sky
