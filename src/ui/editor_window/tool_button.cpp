#include "ui/editor_window/tool_button.hpp"

#include <QPainter>

#include "ui/core/hover_tracker.hpp"
#include "ui/editor_window/key_colors.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr qreal kRadius = 8;
constexpr int kLabelGap = 7;
constexpr int kSidePadding = 12;
constexpr int kFontSize = 13;
constexpr qreal kHoverFill = 0.09;
constexpr qreal kPressFill = 0.16;
constexpr qreal kCheckedFill = 0.16;
constexpr qreal kIdleInk = 0.82;
constexpr qreal kDisabledInk = 0.3;

}  // namespace

ToolButton::ToolButton(const QString &iconId, const QString &tip, QWidget *parent)
    : QAbstractButton(parent), m_iconId(iconId), m_hover(new HoverTracker(this, kHoverFastMs))
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setToolTip(tip);
    setAccessibleName(tip);
    setFixedSize(sizeHint());
    connect(m_hover, &HoverTracker::changed, this, [this] { update(); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void ToolButton::setLabel(const QString &label)
{
    m_label = label;
    setFixedSize(sizeHint());
    update();
}

void ToolButton::setIconId(const QString &iconId)
{
    m_iconId = iconId;
    update();
}

QSize ToolButton::sizeHint() const
{
    if (m_label.isEmpty()) {
        return {kBoxSize, kBoxSize};
    }
    const QFontMetrics metrics(FontLoader::font(kFontSize, QFont::Medium));
    return {kSidePadding * 2 + kIconSize + kLabelGap + metrics.horizontalAdvance(m_label), kBoxSize};
}

void ToolButton::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    const QColor accent = keyAccentColor();
    QPainter painter(this);
    preparePainter(painter);
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const bool enabled = isEnabled();
    qreal fill = enabled ? (isDown() ? kPressFill : kHoverFill * m_hover->progress()) : 0.0;
    QColor ink = withAlpha(theme.fontColor, enabled ? kIdleInk + (1.0 - kIdleInk) * m_hover->progress() : kDisabledInk);
    if (isChecked()) {
        painter.fillPath(roundedPath(box, kRadius), withAlpha(accent, kCheckedFill + fill / 2));
        ink = accent;
    } else if (fill > 0) {
        painter.fillPath(roundedPath(box, kRadius), withAlpha(theme.fontColor, fill));
    }
    const qreal ratio = devicePixelRatioF();
    const QPixmap glyph = IconCache::pixmap(m_iconId, kIconSize, ink, ratio);
    const qreal iconLeft = m_label.isEmpty() ? (width() - kIconSize) / 2.0 : kSidePadding;
    painter.drawPixmap(QPointF(iconLeft, (height() - kIconSize) / 2.0), glyph);
    if (!m_label.isEmpty()) {
        painter.setFont(FontLoader::font(kFontSize, QFont::Medium));
        painter.setPen(ink);
        painter.drawText(QRectF(iconLeft + kIconSize + kLabelGap, 0, width(), height()),
                         Qt::AlignLeft | Qt::AlignVCenter, m_label);
    }
}

}  // namespace sky
