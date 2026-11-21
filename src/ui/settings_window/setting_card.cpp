#include "ui/settings_window/setting_card.hpp"

#include <QComboBox>
#include <QAbstractButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kPadding = 20;
constexpr int kTile = 50;
constexpr qreal kTileRadius = 12;
constexpr int kGlyph = 20;
constexpr int kTextGap = 20;
constexpr int kTitleGap = 5;
constexpr int kWarningGap = 8;
constexpr int kSelectMargin = 15;
constexpr qreal kLift = 2;

ShadowSpec scaled(ShadowSpec shadow, qreal amount)
{
    shadow.color = withAlpha(shadow.color, amount);
    return shadow;
}

}  // namespace

SettingCard::SettingCard(const QString &iconId, const QString &title, const QString &description, QWidget *control,
                         Variant variant, QWidget *parent)
    : CardFrame(kLift, 0, true, parent), m_iconId(iconId), m_variant(variant), m_warning(new QLabel(this))
{
    setPadding(QMargins(kPadding, kPadding, kPadding, kPadding));

    auto *titleLabel = new QLabel(title, this);
    titleLabel->setObjectName(QStringLiteral("SettingTitle"));
    auto *descriptionLabel = new QLabel(this);
    descriptionLabel->setObjectName(QStringLiteral("SettingDescription"));
    descriptionLabel->setTextFormat(Qt::RichText);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setText(QStringLiteral("<div style=\"line-height:20px\">%1</div>").arg(description));
    m_warning->setObjectName(QStringLiteral("SettingWarning"));
    m_warning->setWordWrap(true);
    m_warning->hide();

    auto *copy = new QVBoxLayout();
    copy->setSpacing(0);
    copy->addWidget(titleLabel);
    copy->addSpacing(kTitleGap);
    copy->addWidget(descriptionLabel);
    copy->addWidget(m_warning);
    m_warning->setContentsMargins(0, kWarningGap, 0, 0);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addSpacing(kTile + kTextGap);
    row->addLayout(copy, 1);
    if (control) {
        control->setParent(this);
        const auto *button = qobject_cast<QAbstractButton *>(control);
        if (control->accessibleName().isEmpty() && !(button && !button->text().isEmpty())) {
            control->setAccessibleName(title);
            control->setAccessibleDescription(description);
        }
        if (qobject_cast<QComboBox *>(control)) {
            row->addSpacing(kSelectMargin);
        }
        row->addWidget(control, 0, Qt::AlignVCenter);
    }
    setMinimumHeight(kTile + kPadding * 2 + static_cast<int>(kLift));
}

void SettingCard::setWarningText(const QString &text)
{
    m_warning->setText(text);
    m_warning->setVisible(!text.isEmpty());
}

std::vector<ShadowSpec> SettingCard::hoverShadows() const
{
    const qreal hover = hoverProgress();
    if (hover <= 0) {
        return {};
    }
    const SettingsPalette &s = currentTheme().settings;
    return {scaled(m_variant == Variant::Warning ? s.warnHoverShadow : s.cardHoverShadow, hover)};
}

void SettingCard::paintCard(QPainter &painter, const QRectF &body, qreal hover)
{
    const SettingsPalette &s = currentTheme().settings;
    const QRectF frame = body.adjusted(0.5, 0.5, -0.5, -0.5);
    const QPainterPath path = roundedPath(frame, cornerRadius());
    const bool warning = m_variant == Variant::Warning;
    if (warning) {
        painter.fillPath(path, s.warnFillStart);
    } else {
        painter.fillPath(path, s.cardFill);
    }
    painter.setPen(QPen(warning ? blend(s.warnBorder, s.warnHoverBorder, hover) : blend(s.cardBorder, s.cardHoverBorder, hover), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    const QRectF tile(body.left() + kPadding, body.center().y() - kTile / 2.0, kTile, kTile);
    ShadowPainter::paint(painter, tile, kTileRadius, warning ? s.warnTileShadow : s.tileShadow);
    painter.fillPath(roundedPath(tile, kTileRadius), warning ? s.warnTileStart : s.tileStart);
    painter.drawPixmap(QPointF(tile.center().x() - kGlyph / 2.0, tile.center().y() - kGlyph / 2.0),
                       IconCache::pixmap(m_iconId, kGlyph, s.tileGlyph, devicePixelRatioF()));
}

}  // namespace sky
