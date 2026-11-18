#include "ui/settings_window/shortcut_row.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>

#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/settings_window/shortcut_edit.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kPadding = 16;
constexpr int kIconBox = 24;
constexpr int kIconSize = 20;
constexpr int kGap = 15;
constexpr qreal kShift = 5;

}  // namespace

ShortcutRow::ShortcutRow(const QString &iconId, const QString &label, QWidget *parent)
    : CardFrame(0, kShift, true, parent), m_iconId(iconId), m_icon(new QLabel(this)), m_field(new ShortcutEdit(this))
{
    setPadding(QMargins(kPadding, kPadding, kPadding, kPadding));
    m_icon->setFixedSize(kIconBox, kIconBox);
    m_icon->setAlignment(Qt::AlignCenter);
    auto *text = new QLabel(label, this);
    text->setObjectName(QStringLiteral("ShortcutLabel"));
    m_field->setAccessibleName(label);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(kGap);
    row->addWidget(m_icon);
    row->addWidget(text);
    row->addStretch();
    row->addWidget(m_field);

    refreshIcon();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { refreshIcon(); });
}

void ShortcutRow::refreshIcon()
{
    m_icon->setPixmap(IconCache::pixmap(m_iconId, kIconSize, currentTheme().hoverColor, devicePixelRatioF()));
}

void ShortcutRow::paintCard(QPainter &painter, const QRectF &body, qreal hover)
{
    const SettingsPalette &s = currentTheme().settings;
    const QPainterPath path = roundedPath(body.adjusted(0.5, 0.5, -0.5, -0.5), cornerRadius());
    painter.fillPath(path, s.cardFill);
    painter.setPen(QPen(blend(s.cardBorder, s.cardHoverBorder, hover), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

}  // namespace sky
