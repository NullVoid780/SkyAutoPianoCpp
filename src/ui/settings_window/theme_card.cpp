#include "ui/settings_window/theme_card.hpp"

#include <QFocusEvent>
#include <QFontMetricsF>
#include <QPainter>

#include "core/translator.hpp"
#include "ui/core/hover_tracker.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/widgets/theme_preview.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/icon_button.hpp"

namespace sky {

namespace {

constexpr qreal kRadius = 16;
constexpr int kInset = 10;
constexpr int kPreviewHeight = 140;
constexpr qreal kPreviewRadius = 11;
constexpr int kActionBox = 30;
constexpr int kActionIcon = 15;
constexpr int kActionGap = 2;
constexpr int kTextLeft = 16;
constexpr int kNameTop = kInset + kPreviewHeight + 14;
constexpr int kBadgeHeight = 24;

}  // namespace

ThemeCard::ThemeCard(const ThemeDefinition &definition, QWidget *parent)
    : QAbstractButton(parent),
      m_id(definition.id),
      m_name(definition.name),
      m_subtitle(definition.builtIn ? tr("Built-in") : tr("Custom")),
      m_light(composeTheme(definition, AppTheme::Light)),
      m_dark(composeTheme(definition, AppTheme::Dark)),
      m_hover(new HoverTracker(this, kHoverSlowMs))
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName(definition.name);
    setToolTip(tr("Use %1").arg(definition.name));
    if (definition.builtIn) {
        addAction(QStringLiteral("pencil"), tr("Customize a copy"), Action::Duplicate);
        addAction(QStringLiteral("download"), tr("Export"), Action::Export);
    } else {
        addAction(QStringLiteral("pencil"), tr("Edit"), Action::Edit);
        addAction(QStringLiteral("collection-plus"), tr("Duplicate"), Action::Duplicate);
        addAction(QStringLiteral("download"), tr("Export"), Action::Export);
        addAction(QStringLiteral("trash3"), tr("Delete"), Action::Delete);
    }
    connect(m_hover, &HoverTracker::changed, this, [this] { update(); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        m_previewCache = QPixmap();
        update();
    });
}

void ThemeCard::addAction(const QString &iconId, const QString &tip, Action action)
{
    const bool danger = action == Action::Delete;
    auto *button = new IconButton(
        iconId, kActionIcon, QSize(kActionBox, kActionBox),
        IconButtonStyle{[] { return currentTheme().settings.cardDescText; },
                        [danger] { return danger ? currentTheme().dangerText : currentTheme().hoverColor; },
                        [] { return currentTheme().accent.transparent; }, {}, 8},
        this);
    button->setToolTip(tip);
    button->setAccessibleName(tip);
    connect(button, &QAbstractButton::clicked, this, [this, action] { emit actionRequested(m_id, action); });
    m_actions.push_back(button);
}

void ThemeCard::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        update();
    }
}

QSize ThemeCard::sizeHint() const
{
    return {kMinWidth + 40, kHeight};
}

QSize ThemeCard::minimumSizeHint() const
{
    return {kMinWidth, kHeight};
}

void ThemeCard::resizeEvent(QResizeEvent *event)
{
    QAbstractButton::resizeEvent(event);
    int x = width() - kInset - kActionBox;
    const int y = kNameTop + (height() - kInset - kNameTop - kActionBox) / 2;
    for (auto it = m_actions.rbegin(); it != m_actions.rend(); ++it) {
        (*it)->move(x, y);
        x -= kActionBox + kActionGap;
    }
}

void ThemeCard::focusInEvent(QFocusEvent *event)
{
    QAbstractButton::focusInEvent(event);
    m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
    update();
}

void ThemeCard::focusOutEvent(QFocusEvent *event)
{
    QAbstractButton::focusOutEvent(event);
    m_keyboardFocus = false;
    update();
}

void ThemeCard::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    const SettingsPalette &s = theme.settings;
    QPainter painter(this);
    preparePainter(painter);
    const QRectF frame = QRectF(rect()).adjusted(1, 1, -1, -1);
    const QPainterPath path = roundedPath(frame, kRadius);
    painter.fillPath(path, s.cardFill);
    const qreal hover = m_hover->progress();
    if (m_active) {
        painter.setPen(QPen(theme.hoverColor, 2));
    } else {
        painter.setPen(QPen(blend(s.cardBorder, s.cardHoverBorder, hover), 1));
    }
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    const QRectF preview(kInset, kInset, width() - kInset * 2, kPreviewHeight);
    const qreal dpr = devicePixelRatioF();
    const QSize cacheSize = (preview.size() * dpr).toSize();
    if (m_previewCache.size() != cacheSize || !qFuzzyCompare(m_previewCache.devicePixelRatio(), dpr)) {
        m_previewCache = QPixmap(cacheSize);
        m_previewCache.setDevicePixelRatio(dpr);
        m_previewCache.fill(Qt::transparent);
        QPainter cache(&m_previewCache);
        preparePainter(cache);
        paintThemeSplitPreview(cache, QRectF(QPointF(0, 0), preview.size()), m_light, m_dark, kPreviewRadius, false);
    }
    painter.drawPixmap(preview.topLeft(), m_previewCache);

    if (m_active) {
        const QString label = tr("Active");
        const QFont font = FontLoader::font(12, QFont::DemiBold);
        const qreal textWidth = QFontMetricsF(font).horizontalAdvance(label);
        const qreal badgeWidth = textWidth + 14 + 8 + 20;
        const QRectF badge(preview.left() + 8, preview.top() + 8, badgeWidth, kBadgeHeight);
        const QColor ink = theme.hoverColor.lightnessF() > 0.7 ? theme.accent.ink900 : theme.accent.white;
        paintPill(painter, badge, theme.hoverColor);
        painter.drawPixmap(QPointF(badge.left() + 9, badge.center().y() - 7),
                           IconCache::pixmap(QStringLiteral("check-circle-fill"), 14, ink, devicePixelRatioF()));
        painter.setFont(font);
        painter.setPen(ink);
        painter.drawText(QRectF(badge.left() + 9 + 14 + 6, badge.top(), textWidth + 4, badge.height()),
                         Qt::AlignVCenter | Qt::AlignLeft, label);
    }

    const qreal actionsLeft = m_actions.empty() ? width() - kInset : m_actions.front()->x() - 6;
    const qreal textWidth = std::max<qreal>(0, actionsLeft - kTextLeft);
    const QFont nameFont = FontLoader::font(15, QFont::DemiBold);
    painter.setFont(nameFont);
    painter.setPen(theme.fontColor);
    painter.drawText(QRectF(kTextLeft, kNameTop, textWidth, 22), Qt::AlignVCenter | Qt::AlignLeft,
                     QFontMetricsF(nameFont).elidedText(m_name, Qt::ElideRight, textWidth));
    const QFont subFont = FontLoader::font(12, QFont::Normal);
    painter.setFont(subFont);
    painter.setPen(s.cardDescText);
    painter.drawText(QRectF(kTextLeft, kNameTop + 23, textWidth, 18), Qt::AlignVCenter | Qt::AlignLeft,
                     QFontMetricsF(subFont).elidedText(m_subtitle, Qt::ElideRight, textWidth));

    if (m_keyboardFocus && hasFocus()) {
        paintFocusRing(painter, frame.adjusted(2, 2, -2, -2), kRadius - 2, theme.hoverColor);
    }
}

}  // namespace sky
