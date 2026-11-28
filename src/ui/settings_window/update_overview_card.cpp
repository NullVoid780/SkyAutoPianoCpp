#include "ui/settings_window/update_overview_card.hpp"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include "core/translator.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kPadding = 20;
constexpr qreal kRadius = 16;
constexpr int kTile = 60;
constexpr qreal kTileRadius = 14;
constexpr int kTileGlyph = 28;
constexpr int kTileGap = 18;
constexpr int kPillHeight = 32;
constexpr int kPillPadX = 14;
constexpr int kDot = 8;
constexpr int kDotGap = 8;
constexpr int kTitleGap = 4;
constexpr int kRightGap = 6;

QFont pillFont()
{
    return FontLoader::font(13, QFont::Medium);
}

}  // namespace

UpdateOverviewCard::UpdateOverviewCard(const QString &currentVersion, QWidget *parent)
    : QWidget(parent),
      m_tileSpace(new QWidget(this)),
      m_pillSpace(new QWidget(this)),
      m_version(new QLabel(this)),
      m_currentVersion(currentVersion)
{
    m_tileSpace->setFixedSize(kTile, kTile);
    m_tileSpace->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_pillSpace->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *title = new QLabel(tr("Update"), this);
    title->setObjectName(QStringLiteral("OverviewTitle"));
    auto *text = new QLabel(tr("Keep your app up to date for the latest features and improvements."), this);
    text->setObjectName(QStringLiteral("SettingsRowDescription"));
    text->setWordWrap(true);
    m_version->setObjectName(QStringLiteral("OverviewVersion"));
    m_version->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *copy = new QVBoxLayout();
    copy->setSpacing(kTitleGap);
    copy->addWidget(title);
    copy->addWidget(text);

    auto *status = new QVBoxLayout();
    status->setSpacing(kRightGap);
    status->addWidget(m_pillSpace, 0, Qt::AlignRight);
    status->addWidget(m_version, 0, Qt::AlignRight);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    row->setSpacing(kTileGap);
    row->addWidget(m_tileSpace, 0, Qt::AlignVCenter);
    row->addLayout(copy, 1);
    row->addLayout(status);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
    refresh();
}

void UpdateOverviewCard::setState(State state, const QString &latestVersion)
{
    m_state = state;
    m_latestVersion = latestVersion;
    refresh();
}

QString UpdateOverviewCard::pillText() const
{
    switch (m_state) {
    case State::Checking:
        return tr("Checking...");
    case State::UpToDate:
        return tr("Up to date");
    case State::Available:
        return tr("Update available");
    case State::Failed:
        return tr("Couldn't check");
    case State::Unknown:
        break;
    }
    return tr("Not checked yet");
}

void UpdateOverviewCard::refresh()
{
    const int width = kPillPadX * 2 + kDot + kDotGap + QFontMetrics(pillFont()).horizontalAdvance(pillText());
    m_pillSpace->setFixedSize(width, kPillHeight);
    if (m_state == State::Available && !m_latestVersion.isEmpty()) {
        m_version->setText(tr("Version %1  ·  New %2").arg(m_currentVersion, m_latestVersion));
    } else {
        m_version->setText(tr("Version %1").arg(m_currentVersion));
    }
    update();
}

void UpdateOverviewCard::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    const SettingsPalette &s = theme.settings;
    QPainter painter(this);
    preparePainter(painter);
    const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QPainterPath path = roundedPath(frame, kRadius);
    painter.fillPath(path, s.cardFill);
    painter.setPen(QPen(s.cardBorder, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    const QRectF tile(m_tileSpace->geometry());
    painter.fillPath(roundedPath(tile, kTileRadius), s.subtleFill);
    painter.drawPixmap(QPointF(tile.center().x() - kTileGlyph / 2.0, tile.center().y() - kTileGlyph / 2.0),
                       IconCache::pixmap(QStringLiteral("update"), kTileGlyph, theme.fontColor,
                                         devicePixelRatioF()));

    QColor fill = s.subtleFill;
    QColor border = s.subtleBorder;
    QColor ink = s.cardDescText;
    switch (m_state) {
    case State::UpToDate:
        fill = s.statusSuccessFill;
        border = s.statusSuccessBorder;
        ink = s.statusSuccessText;
        break;
    case State::Available:
        fill = withAlpha(theme.hoverColor, 0.15);
        border = withAlpha(theme.hoverColor, 0.35);
        ink = theme.hoverColor;
        break;
    case State::Failed:
        fill = s.statusErrorFill;
        border = s.statusErrorBorder;
        ink = s.statusErrorText;
        break;
    case State::Unknown:
    case State::Checking:
        break;
    }
    const QRectF pill = QRectF(m_pillSpace->geometry()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.fillPath(roundedPath(pill, pill.height() / 2), fill);
    painter.setPen(QPen(border, 1));
    painter.drawPath(roundedPath(pill, pill.height() / 2));
    const QRectF dot(pill.left() + kPillPadX, pill.center().y() - kDot / 2.0, kDot, kDot);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink);
    painter.drawEllipse(dot);
    painter.setFont(pillFont());
    painter.setPen(ink);
    painter.drawText(pill.adjusted(kPillPadX + kDot + kDotGap, 0, -kPillPadX, 0), Qt::AlignVCenter | Qt::AlignLeft,
                     pillText());
}

}  // namespace sky
