#include "ui/settings_window/key_cap.hpp"

#include <QKeyEvent>
#include <QPainter>

#include "core/string_utils.hpp"
#include "model/key_table.hpp"
#include "ui/core/hover_tracker.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/settings_window/key_capture.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr qreal kCap = 50;
constexpr int kBox = 52;
constexpr qreal kRadius = 10;
constexpr qreal kBorder = 2;
constexpr qreal kHoverScale = 0.05;
constexpr qreal kGlyphWidth = 38;
constexpr qreal kGlyphHeight = 25;
constexpr qreal kGlyphRadius = 6;
constexpr qreal kShape = 25;
constexpr qreal kShapeStroke = 2;
constexpr qreal kSquareRadius = 4;

}  // namespace

KeyCap::KeyCap(int shapes, QWidget *parent)
    : QWidget(parent), m_shapes(shapes), m_hover(new HoverTracker(this, kHoverSlowMs, Easing::Ease))
{
    setFixedSize(kBox, kBox);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    connect(m_hover, &HoverTracker::changed, this, [this] { update(); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

QSize KeyCap::sizeHint() const
{
    return QSize(kBox, kBox);
}

void KeyCap::setValue(const QString &value)
{
    m_value = toQString(normalizeKeyboardMappingValue(fromQString(value)));
    m_label = keyboardDisplayLabel(m_value);
    setToolTip(m_value);
    setAccessibleName(m_label);
    update();
}

void KeyCap::keyPressEvent(QKeyEvent *event)
{
    const auto mapped = keyboardKeyForVirtualKey(event->nativeVirtualKey());
    if (!mapped) {
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
    if (mapped->value != m_value) {
        setValue(mapped->value);
        emit edited();
    }
}

void KeyCap::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    QWidget::mousePressEvent(event);
}

void KeyCap::focusInEvent(QFocusEvent *event)
{
    update();
    QWidget::focusInEvent(event);
}

void KeyCap::focusOutEvent(QFocusEvent *event)
{
    update();
    QWidget::focusOutEvent(event);
}

void KeyCap::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    const SettingsPalette &s = theme.settings;
    QPainter painter(this);
    preparePainter(painter);
    const qreal hover = m_hover->progress();
    const QPointF center = QRectF(rect()).center();
    painter.translate(center);
    painter.scale(1 + kHoverScale * hover, 1 + kHoverScale * hover);

    const QRectF cap(-kCap / 2, -kCap / 2, kCap, kCap);
    painter.fillPath(roundedPath(cap, kRadius), blend(s.keyCapFill, s.keyCapHoverFill, hover));
    painter.setPen(QPen(blend(s.keyCapBorder, theme.hoverColor, hover), kBorder));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(cap.adjusted(kBorder / 2, kBorder / 2, -kBorder / 2, -kBorder / 2), kRadius - kBorder / 2,
                            kRadius - kBorder / 2);

    const QPen shapePen(s.keyShape, kShapeStroke);
    const qreal inner = kShape - kShapeStroke;
    if (m_shapes & Circle) {
        painter.setPen(shapePen);
        painter.drawEllipse(QRectF(-inner / 2, -inner / 2, inner, inner));
    }

    const QRectF glyph(-kGlyphWidth / 2, -kGlyphHeight / 2, kGlyphWidth, kGlyphHeight);
    if (hasFocus()) {
        painter.fillPath(roundedPath(glyph, kGlyphRadius), s.keyGlyphFocusFill);
    }
    painter.setFont(FontLoader::font(11, QFont::DemiBold));
    painter.setPen(theme.fontColor);
    painter.drawText(glyph, Qt::AlignCenter, m_label.toUpper());

    if (m_shapes & Square) {
        painter.save();
        painter.rotate(45);
        painter.setPen(shapePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(-inner / 2, -inner / 2, inner, inner), kSquareRadius, kSquareRadius);
        painter.restore();
    }
}

}  // namespace sky
