#include "ui/main_window/playlist_detail_bar.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "core/translator.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/icon_button.hpp"

namespace sky {

namespace {

constexpr int kSide = 36;
constexpr int kSidePadding = 20;
constexpr int kPadY = 6;

}  // namespace

PlaylistDetailBar::PlaylistDetailBar(QWidget *parent)
    : QWidget(parent),
      m_back(new IconButton(QStringLiteral("arrow-left"), 16, QSize(kSide, kSide),
                            IconButtonStyle{[] { return currentTheme().fontColor; }, [] { return currentTheme().hoverColor; },
                                            [] { return currentTheme().background; }, [] { return currentTheme().neutralBorder; },
                                            10, 0, kHoverMs},
                            this)),
      m_title(new QLabel(this)),
      m_subtitle(new QLabel(this))
{
    m_back->setToolTip(tr("Back"));
    m_back->setAccessibleName(tr("Back"));
    m_title->setObjectName(QStringLiteral("LibraryHeading"));
    m_subtitle->setObjectName(QStringLiteral("PlaylistSubtitle"));
    m_title->setAlignment(Qt::AlignCenter);
    m_subtitle->setAlignment(Qt::AlignCenter);

    auto *heading = new QVBoxLayout();
    heading->setSpacing(0);
    heading->addWidget(m_title);
    heading->addWidget(m_subtitle);

    auto *spacer = new QWidget(this);
    spacer->setFixedSize(kSide, kSide);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(kSidePadding, kPadY, kSidePadding, kPadY);
    row->addWidget(m_back);
    row->addLayout(heading, 1);
    row->addWidget(spacer);

    connect(m_back, &QAbstractButton::clicked, this, &PlaylistDetailBar::backRequested);
}

void PlaylistDetailBar::setPlaylist(const QString &name, int sheetCount)
{
    m_title->setText(name);
    m_subtitle->setText(trCount(sheetCount, trNoop("1 sheet"), trNoop("%1 sheets")));
}

}  // namespace sky
