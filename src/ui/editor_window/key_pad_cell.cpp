#include "ui/editor_window/key_pad_cell.hpp"

#include <QPainter>
#include <QTimer>

#include "ui/core/hover_tracker.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr qreal kRadius = 8;
constexpr qreal kShape = 45 * 0.9;
constexpr qreal kStroke = 2;
constexpr qreal kSquareRadius = 4;
constexpr int kFlashMs = 100;

}  // namespace

KeyPadCell::KeyPadCell(const QString &key, int shapes, QWidget *parent)
    : QAbstractButton(parent),
      m_key(key),
      m_shapes(shapes),
      m_hover(new HoverTracker(this, kHoverSlowMs, Easing::Linear)),
      m_flash(new QTimer(this))
{
    setFixedSize(kSize, kSize);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAccessibleName(key.toUpper());
    m_flash->setSingleShot(true);
    m_flash->setInterval(kFlashMs);
    connect(m_flash, &QTimer::timeout, this, [this] {
        m_flashing = false;
        update();
    });
    connect(m_hover, &HoverTracker::changed, this, [this] { update(); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void KeyPadCell::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        update();
    }
}

void KeyPadCell::flash()
{
    m_flashing = true;
    m_flash->start();
    update();
}

void KeyPadCell::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    const QRectF cell = QRectF(rect());
    QColor fill = blend(theme.gridColor, theme.hoverColor, m_flashing || isDown() ? 1.0 : m_hover->progress());
    if (m_active) {
        fill = theme.activeColor;
    }
    painter.fillPath(roundedPath(cell, kRadius), fill);

    painter.translate(cell.center());
    const QPen pen(theme.fontColor, kStroke);
    const qreal inner = kShape - kStroke;
    painter.setBrush(Qt::NoBrush);
    if (m_shapes & Circle) {
        painter.setPen(pen);
        painter.drawEllipse(QRectF(-inner / 2, -inner / 2, inner, inner));
    }
    if (m_shapes & Square) {
        painter.save();
        painter.rotate(45);
        painter.setPen(pen);
        painter.drawRoundedRect(QRectF(-inner / 2, -inner / 2, inner, inner), kSquareRadius, kSquareRadius);
        painter.restore();
    }
    painter.setFont(FontLoader::font(14, QFont::Bold));
    painter.setPen(m_active ? theme.accent.white : theme.fontColor);
    painter.drawText(QRectF(-kShape / 2, -kShape / 2, kShape, kShape), Qt::AlignCenter, m_key.toUpper());
}

}  // namespace sky
