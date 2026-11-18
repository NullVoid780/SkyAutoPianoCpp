#include "ui/settings_window/app_info_card.hpp"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kPadding = 20;
constexpr qreal kRadius = 16;
constexpr int kLogo = 64;
constexpr int kLogoGap = 18;
constexpr int kLineGap = 3;

}  // namespace

AppInfoCard::AppInfoCard(const QString &version, QWidget *parent) : QWidget(parent), m_logoSpace(new QWidget(this))
{
    m_logoSpace->setFixedSize(kLogo, kLogo);
    m_logoSpace->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *name = new QLabel(QStringLiteral("Sky Auto Piano"), this);
    name->setObjectName(QStringLiteral("OverviewTitle"));
    auto *versionLabel = new QLabel(QStringLiteral("Version %1").arg(version), this);
    versionLabel->setObjectName(QStringLiteral("OverviewVersion"));
    auto *tagline = new QLabel(QStringLiteral("Plays music sheets for you in Sky: Children of the Light."), this);
    tagline->setObjectName(QStringLiteral("SettingsRowDescription"));
    tagline->setWordWrap(true);

    auto *copy = new QVBoxLayout();
    copy->setSpacing(kLineGap);
    copy->addWidget(name);
    copy->addWidget(versionLabel);
    copy->addSpacing(4);
    copy->addWidget(tagline);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    row->setSpacing(kLogoGap);
    row->addWidget(m_logoSpace, 0, Qt::AlignVCenter);
    row->addLayout(copy, 1);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void AppInfoCard::paintEvent(QPaintEvent *)
{
    const SettingsPalette &s = currentTheme().settings;
    QPainter painter(this);
    preparePainter(painter);
    const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QPainterPath path = roundedPath(frame, kRadius);
    painter.fillPath(path, s.cardFill);
    painter.setPen(QPen(s.cardBorder, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.drawPixmap(m_logoSpace->geometry(), QIcon(QStringLiteral(":/icon/Icon9.ico")).pixmap(kLogo, kLogo));
}

}  // namespace sky
