#include "ui/editor_window/key_pad_cell.hpp"

#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>

#include "ui/core/hover_tracker.hpp"
#include "ui/editor_window/key_colors.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr qreal kRadius = 8;
constexpr qreal kShape = 45 * 0.9;
constexpr qreal kStroke = 2;
constexpr qreal kSquareRadius = 4;
constexpr int kFontSize = 14;
constexpr int kMinFontSize = 10;
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

void KeyPadCell::setKeyIndex(int index)
{
    m_keyIndex = index;
    update();
}

void KeyPadCell::setCellSize(int size)
{
    if (size != m_size) {
        m_size = size;
        setFixedSize(size, size);
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
    const qreal scale = cell.width() / kSize;
    const qreal shape = kShape * scale;
    QColor fill = blend(theme.gridColor, theme.hoverColor, m_flashing || isDown() ? 1.0 : m_hover->progress());
    const bool keyed = m_keyIndex >= 0;
    if (m_active) {
        fill = keyed ? keyRowColor(m_keyIndex) : theme.activeColor;
    }
    painter.fillPath(roundedPath(cell, kRadius * scale), fill);

    painter.translate(cell.center());
    const QColor ink = m_active && keyed ? keyInkColor(fill) : theme.fontColor;
    const QPen pen(ink, kStroke);
    const qreal inner = shape - kStroke;
    painter.setBrush(Qt::NoBrush);
    if (m_shapes & Circle) {
        painter.setPen(pen);
        painter.drawEllipse(QRectF(-inner / 2, -inner / 2, inner, inner));
    }
    if (m_shapes & Square) {
        painter.save();
        painter.rotate(45);
        painter.setPen(pen);
        painter.drawRoundedRect(QRectF(-inner / 2, -inner / 2, inner, inner), kSquareRadius * scale,
                                kSquareRadius * scale);
        painter.restore();
    }
    painter.setFont(FontLoader::font(std::max(kMinFontSize, static_cast<int>(std::lround(kFontSize * scale))), QFont::Bold));
    painter.setPen(m_active && keyed ? ink : (m_active ? theme.accent.white : theme.fontColor));
    painter.drawText(QRectF(-shape / 2, -shape / 2, shape, shape), Qt::AlignCenter, m_key.toUpper());
}

}  // namespace sky
