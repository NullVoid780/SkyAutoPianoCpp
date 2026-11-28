#include "ui/settings_window/save_button.hpp"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QPainter>

#include "core/translator.hpp"
#include "ui/core/hover_tracker.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kGlyph = 24;

}  // namespace

SaveButton::SaveButton(QWidget *parent) : QAbstractButton(parent), m_hover(new HoverTracker(this, kHoverSlowMs, Easing::Ease))
{
    setFixedSize(kDiameter, kDiameter + kLift);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(tr("Save"));
    setAccessibleName(tr("Save"));
    connect(m_hover, &HoverTracker::changed, this, [this] {
        update();
        emit bodyMoved();
    });
    connect(this, &QAbstractButton::pressed, this, [this] {
        update();
        emit bodyMoved();
    });
    connect(this, &QAbstractButton::released, this, [this] {
        update();
        emit bodyMoved();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

QRectF SaveButton::bodyRect() const
{
    const qreal lift = isDown() ? 0.0 : kLift * m_hover->progress();
    return QRectF(0, kLift - lift, kDiameter, kDiameter);
}

bool SaveButton::hitButton(const QPoint &pos) const
{
    const QRectF body = bodyRect();
    const QPointF delta = QPointF(pos) - body.center();
    return delta.x() * delta.x() + delta.y() * delta.y() <= (kDiameter / 2.0) * (kDiameter / 2.0);
}

void SaveButton::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        click();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

void SaveButton::focusInEvent(QFocusEvent *event)
{
    m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
    QAbstractButton::focusInEvent(event);
}

void SaveButton::focusOutEvent(QFocusEvent *event)
{
    m_keyboardFocus = false;
    QAbstractButton::focusOutEvent(event);
}

void SaveButton::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    const SettingsPalette &s = theme.settings;
    QPainter painter(this);
    preparePainter(painter);
    const QRectF body = bodyRect();
    painter.setPen(Qt::NoPen);
    painter.setBrush(s.saveStart);
    painter.drawEllipse(body);
    painter.drawPixmap(QPointF(body.center().x() - kGlyph / 2.0, body.center().y() - kGlyph / 2.0),
                       IconCache::pixmap(QStringLiteral("floppy"), kGlyph, theme.accent.white, devicePixelRatioF()));
    if (hasFocus() && m_keyboardFocus) {
        paintFocusRing(painter, body.adjusted(4, 4, -4, -4), kDiameter / 2.0 - 4, theme.accent.white);
    }
}

}  // namespace sky
