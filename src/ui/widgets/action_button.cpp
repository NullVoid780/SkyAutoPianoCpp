#include "ui/widgets/action_button.hpp"

#include <QFocusEvent>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>

#include "ui/core/elevation_layer.hpp"
#include "ui/core/hover_tracker.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/card_frame.hpp"

namespace sky {

namespace {

constexpr int kSpinnerFrameMs = 16;
constexpr qreal kSpinnerPeriodMs = 2000;
constexpr qreal kSpinnerStroke = 5;
constexpr qreal kSpinnerSize = 50;
constexpr qreal kSpinnerPixels = 20;

const SettingsPalette &settingsPalette()
{
    return currentTheme().settings;
}

}  // namespace

ActionButtonStyle ActionButton::styleFor(Kind kind)
{
    ActionButtonStyle style;
    const auto white = [] { return currentTheme().accent.white; };
    switch (kind) {
    case Kind::Primary:
        style.fillStart = [] { return currentTheme().hoverColor; };
        style.fillEnd = [] { return settingsPalette().tileEnd; };
        style.text = white;
        break;
    case Kind::Secondary:
        style.fillStart = [] { return settingsPalette().importFill; };
        style.hoverFill = [] { return settingsPalette().importHoverFill; };
        style.border = [] { return settingsPalette().importBorder; };
        style.text = [] { return settingsPalette().importText; };
        break;
    case Kind::Check:
        style.fillStart = [] { return settingsPalette().checkStart; };
        style.fillEnd = [] { return settingsPalette().checkEnd; };
        style.text = white;
        style.minHeight = 38;
        style.padX = 14;
        style.iconSize = 18;
        style.gap = 12;
        style.iconSpin = 180;
        break;
    case Kind::Cancel:
        style.fillStart = [] { return settingsPalette().cancelFill; };
        style.border = [] { return settingsPalette().cancelBorder; };
        style.text = [] { return settingsPalette().cancelText; };
        style.radius = 10;
        style.minWidth = 120;
        break;
    case Kind::Confirm:
        style.fillStart = [] { return settingsPalette().warnTileStart; };
        style.fillEnd = [] { return settingsPalette().warnTileEnd; };
        style.text = white;
        style.radius = 10;
        style.minWidth = 120;
        break;
    case Kind::UpdateNow:
        style.fillStart = [] { return currentTheme().hoverColor; };
        style.fillEnd = [] { return settingsPalette().tileEnd; };
        style.text = white;
        style.minWidth = 140;
        break;
    case Kind::Later:
        style.fillStart = [] { return currentTheme().accent.transparent; };
        style.border = [] { return currentTheme().update.laterBorder; };
        style.borderWidth = 2;
        style.text = [] { return currentTheme().update.laterText; };
        style.minWidth = 140;
        break;
    case Kind::Subtle:
        style.fillStart = [] { return settingsPalette().subtleFill; };
        style.hoverFill = [] { return settingsPalette().subtleHoverFill; };
        style.border = [] { return settingsPalette().subtleBorder; };
        style.text = [] { return currentTheme().fontColor; };
        style.radius = 10;
        style.minHeight = 40;
        style.padX = 16;
        style.iconSize = 15;
        style.lift = 0;
        break;
    case Kind::Amber:
        style.fillStart = [] { return settingsPalette().amberSolid; };
        style.hoverFill = [] { return settingsPalette().amberSolidHover; };
        style.text = white;
        style.radius = 10;
        style.minHeight = 44;
        style.minWidth = 200;
        style.padX = 18;
        style.iconSize = 16;
        style.fontSize = 14;
        style.lift = 0;
        break;
    }
    return style;
}

void ActionButton::setIconId(const QString &iconId)
{
    m_iconId = iconId;
    updateGeometry();
    update();
}

void ActionButton::setBusy(bool busy, const QString &busyLabel)
{
    if (busy == m_busy) {
        return;
    }
    m_busy = busy;
    if (busy) {
        m_idleLabel = text();
        setText(busyLabel);
        m_spinner->start();
    } else {
        setText(m_idleLabel);
        m_spinner->stop();
    }
    setEnabled(!busy);
    updateGeometry();
    update();
}

ActionButton::ActionButton(Kind kind, const QString &iconId, const QString &label, QWidget *parent)
    : QAbstractButton(parent),
      m_style(styleFor(kind)),
      m_iconId(iconId),
      m_hover(new HoverTracker(this, kHoverSlowMs, Easing::Ease)),
      m_spinner(new QTimer(this))
{
    setText(label);
    m_spinner->setInterval(kSpinnerFrameMs);
    connect(m_spinner, &QTimer::timeout, this, [this] {
        m_spin = std::fmod(m_spin + 360.0 * kSpinnerFrameMs / kSpinnerPeriodMs, 360.0);
        update();
    });
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_hover, &HoverTracker::changed, this, [this] {
        update();
        if (m_layer) {
            m_layer->refresh(this);
        }
    });
    connect(this, &QAbstractButton::pressed, this, [this] { update(); });
    connect(this, &QAbstractButton::released, this, [this] { update(); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void ActionButton::attachShadows(ElevationLayer *layer)
{
    if (!m_style.shadow) {
        return;
    }
    m_layer = layer;
    layer->track(this, m_style.radius, [this] {
        if (!isEnabled()) {
            return std::vector<ShadowSpec>{};
        }
        return std::vector<ShadowSpec>{m_style.shadow()};
    }, [this] { return bodyRect(); });
}

void ActionButton::attachShadows(CardFrame *card)
{
    if (!m_style.shadow) {
        return;
    }
    card->addChildShadow(this, m_style.radius, [this]() -> std::optional<ShadowSpec> {
        if (!isEnabled()) {
            return std::nullopt;
        }
        return m_style.shadow();
    });
}

QFont ActionButton::labelFont() const
{
    return FontLoader::font(m_style.fontSize, QFont::DemiBold);
}

QSize ActionButton::sizeHint() const
{
    const qreal textWidth = QFontMetricsF(labelFont()).horizontalAdvance(text());
    const qreal iconSpace = m_iconId.isEmpty() && !m_busy ? 0.0 : m_style.iconSize + m_style.gap;
    const int width = static_cast<int>(std::ceil(m_style.padX * 2 + iconSpace + textWidth));
    return QSize(std::max(width, m_style.minWidth), m_style.minHeight + static_cast<int>(m_style.lift));
}

qreal ActionButton::liftAmount() const
{
    return isEnabled() && !isDown() ? m_style.lift * m_hover->progress() : 0.0;
}

QRectF ActionButton::bodyRect() const
{
    const qreal lift = liftAmount();
    return QRectF(0, m_style.lift - lift, width(), height() - m_style.lift);
}

void ActionButton::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        click();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

void ActionButton::focusInEvent(QFocusEvent *event)
{
    m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
    QAbstractButton::focusInEvent(event);
}

void ActionButton::focusOutEvent(QFocusEvent *event)
{
    m_keyboardFocus = false;
    QAbstractButton::focusOutEvent(event);
}

void ActionButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    preparePainter(painter);
    const qreal hover = isEnabled() ? m_hover->progress() : 0.0;
    const QRectF body = bodyRect();
    const QPainterPath path = roundedPath(body.adjusted(0.5, 0.5, -0.5, -0.5), m_style.radius);
    if (!isEnabled()) {
        painter.setOpacity(0.6);
    }
    const QColor base = m_style.fillStart();
    const QColor hovered = m_style.hoverFill ? m_style.hoverFill() : base.darker(112);
    painter.fillPath(path, blend(base, hovered, hover));
    if (m_style.border) {
        painter.setPen(QPen(m_style.border(), m_style.borderWidth));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    const QColor ink = m_style.text();
    const QFont font = labelFont();
    const qreal textWidth = QFontMetricsF(font).horizontalAdvance(text());
    const qreal iconSpace = m_iconId.isEmpty() && !m_busy ? 0.0 : m_style.iconSize + m_style.gap;
    const qreal contentWidth = iconSpace + textWidth;
    const qreal left = body.center().x() - contentWidth / 2;
    const QPointF iconCenter(left + m_style.iconSize / 2.0, body.center().y());
    painter.save();
    painter.translate(iconCenter);
    if (m_busy) {
        painter.rotate(m_spin);
        painter.setPen(QPen(ink, kSpinnerStroke * kSpinnerPixels / kSpinnerSize, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        const qreal r = kSpinnerPixels * 20.0 / kSpinnerSize;
        painter.drawArc(QRectF(-r, -r, r * 2, r * 2), 0, 270 * 16);
    } else if (!m_iconId.isEmpty()) {
        painter.rotate(m_style.iconSpin * hover);
        painter.drawPixmap(QPointF(-m_style.iconSize / 2.0, -m_style.iconSize / 2.0),
                           IconCache::pixmap(m_iconId, m_style.iconSize, ink, devicePixelRatioF()));
    }
    painter.restore();
    painter.setFont(font);
    painter.setPen(ink);
    painter.drawText(QRectF(left + iconSpace, body.top(), textWidth + 2, body.height()),
                     Qt::AlignVCenter | Qt::AlignLeft, text());

    if (hasFocus() && m_keyboardFocus) {
        painter.setOpacity(1.0);
        paintFocusRing(painter, body.adjusted(3, 3, -3, -3), m_style.radius - 3, ink);
    }
}

}  // namespace sky
