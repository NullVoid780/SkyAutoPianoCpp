#include "ui/widgets/badge.hpp"

#include <QFontMetrics>
#include <QPainter>

#include "ui/render/font_loader.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kHeight = 22;
constexpr int kPadding = 8;

QFont badgeFont()
{
    return FontLoader::font(12, QFont::Bold);
}

}  // namespace

Badge::Badge(const QString &text, QWidget *parent) : QWidget(parent), m_text(text)
{
    setFixedSize(sizeHint());
}

QSize Badge::sizeHint() const
{
    return QSize(QFontMetrics(badgeFont()).horizontalAdvance(m_text) + kPadding * 2, kHeight);
}

void Badge::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    paintPill(painter, rect(), theme.holdBadgeFill);
    painter.setFont(badgeFont());
    painter.setPen(theme.fontColor);
    painter.drawText(rect(), Qt::AlignCenter, m_text);
}

}  // namespace sky
