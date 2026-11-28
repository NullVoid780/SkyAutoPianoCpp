#include "ui/main_window/bulk_toolbar.hpp"

#include <QBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>

#include "core/translator.hpp"
#include "core/string_utils.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/icon_button.hpp"

namespace sky {

namespace {

constexpr int kPadY = 8;
constexpr int kPadX = 12;
constexpr int kGap = 10;
constexpr int kActionGap = 6;
constexpr qreal kRadius = 14;
constexpr int kStackBreakpoint = 720;
constexpr int kStackedActionWidth = 140;

QPushButton *makeButton(const QString &label, const char *objectName, QWidget *parent)
{
    auto *button = new QPushButton(label, parent);
    button->setObjectName(QLatin1String(objectName));
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

}  // namespace

BulkToolbar::BulkToolbar(QWidget *parent)
    : QWidget(parent),
      m_selectAll(new IconButton(QStringLiteral("check-square"), 18, QSize(20, 20),
                                 IconButtonStyle{[this] { return m_allSelected ? currentTheme().accent.blue600 : currentTheme().fontColor; },
                                                 {}, {}, {}, 0, 0, kHoverMs},
                                 this)),
      m_summary(new QLabel(this)),
      m_playlists(new QComboBox(this)),
      m_add(makeButton(tr("Add to playlist"), "PrimaryButton", this)),
      m_delete(makeButton(tr("Delete"), "DestructiveButton", this)),
      m_done(makeButton(tr("Done"), "NeutralButton", this))
{
    setObjectName(QStringLiteral("BulkToolbar"));
    m_summary->setObjectName(QStringLiteral("BulkSummary"));
    m_summary->setTextFormat(Qt::RichText);
    m_playlists->setObjectName(QStringLiteral("BulkPlaylistSelect"));
    m_playlists->setMinimumWidth(130);
    m_playlists->setAccessibleName(tr("Target playlist"));

    connect(m_selectAll, &QAbstractButton::clicked, this, &BulkToolbar::selectAllRequested);
    connect(m_add, &QPushButton::clicked, this, &BulkToolbar::addToPlaylistRequested);
    connect(m_delete, &QPushButton::clicked, this, &BulkToolbar::deleteRequested);
    connect(m_done, &QPushButton::clicked, this, &BulkToolbar::doneRequested);
    connect(m_playlists, &QComboBox::currentIndexChanged, this, [this] { emit playlistChosen(selectedPlaylistId()); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });

    applyLayoutForWidth(800);
    setState(0, false, false);
}

void BulkToolbar::applyLayoutForWidth(int width)
{
    const bool stacked = width < kStackBreakpoint;
    if (layout() && stacked == m_stacked) {
        return;
    }
    m_stacked = stacked;
    delete layout();

    auto *left = new QHBoxLayout();
    left->setSpacing(8);
    left->addWidget(m_selectAll);
    left->addWidget(m_summary);
    left->addStretch();

    auto *actions = new QBoxLayout(QBoxLayout::LeftToRight);
    actions->setSpacing(kActionGap);
    for (QWidget *widget : {static_cast<QWidget *>(m_playlists), static_cast<QWidget *>(m_add),
                            static_cast<QWidget *>(m_delete), static_cast<QWidget *>(m_done)}) {
        widget->setMinimumWidth(stacked ? kStackedActionWidth : (widget == m_playlists ? 130 : 0));
        actions->addWidget(widget, stacked ? 1 : 0);
    }

    auto *root = new QBoxLayout(stacked ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight, this);
    root->setContentsMargins(kPadX, kPadY, kPadX, kPadY);
    root->setSpacing(kGap);
    root->addLayout(left, 1);
    root->addLayout(actions);
}

void BulkToolbar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    applyLayoutForWidth(window()->width());
}

void BulkToolbar::setState(int selectedCount, bool allVisibleSelected, bool anyVisible)
{
    const Theme &theme = currentTheme();
    m_allSelected = allVisibleSelected;
    m_selectAll->setIconId(allVisibleSelected ? QStringLiteral("check-square-ticked") : QStringLiteral("check-square"));
    m_selectAll->setEnabled(anyVisible);
    const QString hint = allVisibleSelected ? tr("Clear visible selection") : tr("Select all visible sheets");
    m_selectAll->setToolTip(hint);
    m_selectAll->setAccessibleName(hint);
    m_summary->setText(tr("<span style=\"color:%1; font-weight:700\">%2</span> selected")
                           .arg(theme.bulkCount.name())
                           .arg(selectedCount));
    m_add->setEnabled(selectedCount > 0 && m_playlists->isEnabled());
    m_delete->setEnabled(selectedCount > 0);
}

void BulkToolbar::setPlaylists(const std::vector<Playlist> &playlists, const std::string &selectedId)
{
    const QSignalBlocker blocker(m_playlists);
    m_playlists->clear();
    if (playlists.empty()) {
        m_playlists->addItem(tr("No playlists"), QString());
        m_playlists->setEnabled(false);
        m_add->setEnabled(false);
        return;
    }
    m_playlists->setEnabled(true);
    for (const Playlist &playlist : playlists) {
        m_playlists->addItem(toQString(playlist.name), toQString(playlist.id));
        if (playlist.id == selectedId) {
            m_playlists->setCurrentIndex(m_playlists->count() - 1);
        }
    }
}

std::string BulkToolbar::selectedPlaylistId() const
{
    return fromQString(m_playlists->currentData().toString());
}

void BulkToolbar::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QPainterPath path = roundedPath(box, kRadius);
    painter.fillPath(path, theme.bulkFill);
    painter.setPen(QPen(theme.bulkBorder, 1));
    painter.drawPath(path);
}

}  // namespace sky
