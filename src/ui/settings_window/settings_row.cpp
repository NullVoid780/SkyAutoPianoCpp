#include "ui/settings_window/settings_row.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kPadX = 18;
constexpr int kPadY = 16;
constexpr qreal kRadius = 14;
constexpr int kPlainIcon = 22;
constexpr int kTile = 44;
constexpr qreal kTileRadius = 12;
constexpr int kTileGlyph = 22;
constexpr int kIconGap = 16;
constexpr int kTitleGap = 3;

}  // namespace

SettingsRow::SettingsRow(const QString &iconId, const QString &title, const QString &description, QWidget *control,
                         QWidget *parent)
    : QWidget(parent), m_iconId(iconId), m_iconSpace(new QWidget(this))
{
    setAttribute(Qt::WA_Hover);
    m_iconSpace->setFixedSize(kPlainIcon, kPlainIcon);
    m_iconSpace->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *titleLabel = new QLabel(title, this);
    titleLabel->setObjectName(QStringLiteral("SettingsRowTitle"));
    auto *descriptionLabel = new QLabel(description, this);
    descriptionLabel->setObjectName(QStringLiteral("SettingsRowDescription"));
    descriptionLabel->setWordWrap(true);

    auto *copy = new QVBoxLayout();
    copy->setSpacing(kTitleGap);
    copy->addWidget(titleLabel);
    copy->addWidget(descriptionLabel);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(kPadX, kPadY, kPadX, kPadY);
    row->setSpacing(kIconGap);
    row->addWidget(m_iconSpace, 0, Qt::AlignVCenter);
    row->addLayout(copy, 1);
    if (control) {
        control->setParent(this);
        row->addWidget(control, 0, Qt::AlignVCenter);
    }
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void SettingsRow::setIconTile(std::function<QColor()> fill, std::function<QColor()> glyph)
{
    m_tileFill = std::move(fill);
    m_tileGlyph = std::move(glyph);
    m_iconSpace->setFixedSize(kTile, kTile);
    update();
}

void SettingsRow::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_hovered = true;
    update();
}

void SettingsRow::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_hovered = false;
    update();
}

void SettingsRow::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    const SettingsPalette &s = theme.settings;
    QPainter painter(this);
    preparePainter(painter);
    const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QPainterPath path = roundedPath(frame, kRadius);
    painter.fillPath(path, s.cardFill);
    painter.setPen(QPen(m_hovered ? s.cardHoverBorder : s.cardBorder, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    const QRectF icon(m_iconSpace->geometry());
    const qreal dpr = devicePixelRatioF();
    if (m_tileFill) {
        painter.fillPath(roundedPath(icon, kTileRadius), m_tileFill());
        painter.drawPixmap(QPointF(icon.center().x() - kTileGlyph / 2.0, icon.center().y() - kTileGlyph / 2.0),
                           IconCache::pixmap(m_iconId, kTileGlyph, m_tileGlyph(), dpr));
    } else {
        painter.drawPixmap(icon.topLeft(), IconCache::pixmap(m_iconId, kPlainIcon, theme.fontColor, dpr));
    }
}

}  // namespace sky
