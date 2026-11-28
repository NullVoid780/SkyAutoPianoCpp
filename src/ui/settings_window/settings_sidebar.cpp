#include "ui/settings_window/settings_sidebar.hpp"

#include <QIcon>
#include <QPainter>
#include <QVBoxLayout>

#include "core/translator.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/settings_window/sidebar_item.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kSide = 16;
constexpr int kHeaderTop = 24;
constexpr int kLogo = 36;
constexpr int kTitleGap = 12;
constexpr int kListTop = 28;
constexpr int kItemGap = 4;
constexpr int kDividerGap = 8;
constexpr int kCardHeight = 64;
constexpr int kCardBottom = 16;
constexpr qreal kCardRadius = 12;
constexpr int kCardPadX = 16;

}  // namespace

SettingsSidebar::SettingsSidebar(const QString &version, QWidget *parent)
    : QWidget(parent), m_version(version)
{
    setFixedWidth(kWidth);
    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(kSide, kHeaderTop + kLogo + kListTop, kSide + 1, kCardHeight + kCardBottom * 2);
    column->setSpacing(kItemGap);
    column->addStretch();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

int SettingsSidebar::addItem(const QString &iconId, const QString &label)
{
    auto *item = new SidebarItem(iconId, label, this);
    const int index = static_cast<int>(m_items.size());
    auto *column = static_cast<QVBoxLayout *>(layout());
    column->insertWidget(column->count() - 1, item);
    m_items.push_back(item);
    connect(item, &QAbstractButton::clicked, this, [this, index] { setCurrent(index); });
    return index;
}

void SettingsSidebar::addSeparator()
{
    auto *line = new QWidget(this);
    line->setObjectName(QStringLiteral("SidebarDivider"));
    line->setAttribute(Qt::WA_StyledBackground);
    line->setFixedHeight(1);
    auto *column = static_cast<QVBoxLayout *>(layout());
    column->insertSpacing(column->count() - 1, kDividerGap);
    column->insertWidget(column->count() - 1, line);
    column->insertSpacing(column->count() - 1, kDividerGap);
}

void SettingsSidebar::setCurrent(int index)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return;
    }
    m_items[static_cast<std::size_t>(index)]->setChecked(true);
    if (index == m_current) {
        return;
    }
    m_current = index;
    emit currentChanged(index);
}

void SettingsSidebar::setBadge(int index, bool visible)
{
    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        m_items[static_cast<std::size_t>(index)]->setBadge(visible);
    }
}

QRectF SettingsSidebar::versionCard() const
{
    return QRectF(kSide, height() - kCardBottom - kCardHeight, width() - 1 - kSide * 2, kCardHeight);
}

void SettingsSidebar::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    const SettingsPalette &s = theme.settings;
    QPainter painter(this);
    preparePainter(painter);
    painter.fillRect(rect(), s.sidebarFill);
    painter.fillRect(QRectF(width() - 1, 0, 1, height()), s.sidebarEdge);

    const QRectF logo(kSide + 4, kHeaderTop, kLogo, kLogo);
    painter.drawPixmap(logo.toRect(), QIcon(QStringLiteral(":/icon/Icon9.ico")).pixmap(kLogo, kLogo));
    painter.setFont(FontLoader::font(20, QFont::DemiBold));
    painter.setPen(theme.fontColor);
    painter.drawText(QRectF(logo.right() + kTitleGap, logo.top(), width() - logo.right(), kLogo),
                     Qt::AlignVCenter | Qt::AlignLeft, tr("Settings"));

    const QRectF card = versionCard();
    painter.fillPath(roundedPath(card, kCardRadius), s.subtleFill);
    const qreal textLeft = card.left() + kCardPadX;
    const QRectF upper(textLeft, card.top() + 12, card.right() - textLeft, card.height() / 2 - 12);
    const QRectF lower(textLeft, card.center().y(), card.right() - textLeft, card.height() / 2 - 10);
    painter.setFont(FontLoader::font(12, QFont::Normal));
    painter.setPen(s.cardDescText);
    painter.drawText(upper, Qt::AlignLeft | Qt::AlignBottom, tr("App Version"));
    painter.setFont(FontLoader::font(16, QFont::Medium));
    painter.setPen(theme.fontColor);
    painter.drawText(lower, Qt::AlignLeft | Qt::AlignTop, m_version);
}

}  // namespace sky
