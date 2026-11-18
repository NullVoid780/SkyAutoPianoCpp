#include "ui/settings_window/tab_header.hpp"

#include <QBoxLayout>
#include <QFontMetricsF>
#include <QLabel>
#include <QPainter>

#include <algorithm>
#include <cmath>

#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kIconSize = 20;
constexpr int kIconGap = 12;
constexpr int kTitleBelow = 8;
constexpr int kRowGap = 16;
constexpr int kActionGap = 10;
constexpr int kBottomPadding = 15;

}  // namespace

TabHeader::TabHeader(const QString &iconId, std::function<QColor()> accent, const QString &title,
                     const QString &description, QWidget *parent)
    : QWidget(parent),
      m_iconId(iconId),
      m_accent(std::move(accent)),
      m_descriptionText(description),
      m_icon(new QLabel(this)),
      m_description(new QLabel(this)),
      m_copy(new QWidget(this)),
      m_actions(new QWidget(this)),
      m_actionRow(new QHBoxLayout(m_actions)),
      m_row(new QBoxLayout(QBoxLayout::LeftToRight))
{
    auto *heading = new QLabel(title, m_copy);
    heading->setObjectName(QStringLiteral("SettingsHeading"));
    m_icon->setFixedSize(kIconSize, kIconSize);
    m_description->setObjectName(QStringLiteral("SettingsHeaderDescription"));
    m_description->setTextFormat(Qt::RichText);
    m_description->setWordWrap(true);
    m_description->setText(QStringLiteral("<div style=\"line-height:24px\">%1</div>").arg(description));
    m_description->setParent(m_copy);

    auto *headingRow = new QHBoxLayout();
    headingRow->setSpacing(kIconGap);
    headingRow->addWidget(m_icon, 0, Qt::AlignVCenter);
    headingRow->addWidget(heading, 0, Qt::AlignVCenter);
    headingRow->addStretch();
    m_icon->setParent(m_copy);

    auto *copy = new QVBoxLayout(m_copy);
    copy->setContentsMargins(0, 0, 0, 0);
    copy->setSpacing(kTitleBelow);
    copy->addLayout(headingRow);
    copy->addWidget(m_description);

    m_actionRow->setContentsMargins(0, 0, 0, 0);
    m_actionRow->setSpacing(kActionGap);
    m_actions->hide();

    m_row->setSpacing(kRowGap);
    m_row->addWidget(m_copy, 1, Qt::AlignTop);
    m_row->addWidget(m_actions, 0, Qt::AlignTop);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, kBottomPadding + 1);
    root->addLayout(m_row);

    refreshIcon();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        refreshIcon();
        update();
    });
}

void TabHeader::addAction(QWidget *action)
{
    action->setParent(m_actions);
    m_actionRow->addWidget(action);
    m_actions->show();
    updateWrap();
}

void TabHeader::refreshIcon()
{
    m_icon->setPixmap(IconCache::pixmap(m_iconId, kIconSize, m_accent(), devicePixelRatioF()));
}

void TabHeader::updateWrap()
{
    if (m_actions->isHidden() || width() <= 0) {
        return;
    }
    const qreal descriptionWidth = QFontMetricsF(FontLoader::font(16, QFont::Light)).horizontalAdvance(m_descriptionText);
    const int copyWidth = static_cast<int>(std::ceil(descriptionWidth)) + 2;
    const bool wrap = copyWidth + kRowGap + m_actions->sizeHint().width() > width();
    if (wrap == m_wrapped) {
        return;
    }
    m_wrapped = wrap;
    m_row->setDirection(wrap ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    m_row->setAlignment(m_actions, wrap ? Qt::AlignLeft : Qt::AlignTop);
}

void TabHeader::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateWrap();
}

void TabHeader::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(QRectF(0, height() - 1, width(), 1), currentTheme().settings.headerRule);
}

}  // namespace sky
