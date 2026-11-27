#include "ui/editor_window/meta_panel.hpp"

#include <QPainter>
#include <QVBoxLayout>

#include "ui/editor_window/editable_field.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kPadding = 15;
constexpr int kRowGap = 6;
constexpr qreal kRadius = 12;

}  // namespace

MetaPanel::MetaPanel(QWidget *parent) : QWidget(parent)
{
    const std::array<QString, 4> labels = {QStringLiteral("Name:"), QStringLiteral("Author:"),
                                           QStringLiteral("Transcribed by:"),
                                           QStringLiteral("BPM:")};
    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    column->setSpacing(kRowGap);
    for (std::size_t i = 0; i < labels.size(); ++i) {
        m_fields[i] = new EditableField(labels[i], this);
        column->addWidget(m_fields[i]);
    }
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void MetaPanel::paintEvent(QPaintEvent *)
{
    const EditorPalette &e = currentTheme().editor;
    QPainter painter(this);
    preparePainter(painter);
    const QPainterPath path = roundedPath(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kRadius);
    painter.fillPath(path, e.metaFill);
    painter.setPen(QPen(e.metaBorder, 1));
    painter.drawPath(path);
}

}  // namespace sky
