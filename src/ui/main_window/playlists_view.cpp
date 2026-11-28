#include "ui/main_window/playlists_view.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <set>

#include "core/translator.hpp"
#include "core/string_utils.hpp"
#include "services/library/library_service.hpp"
#include "services/library/playlist_service.hpp"
#include "ui/main_window/playlist_card_delegate.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/state_card.hpp"

namespace sky {

namespace {

constexpr int kSidePadding = 20;
constexpr int kToolbarPadY = 6;

}  // namespace

PlaylistsView::PlaylistsView(PlaylistService &playlists, LibraryService &library, QWidget *parent)
    : QWidget(parent),
      m_playlists(playlists),
      m_library(library),
      m_model(new QStandardItemModel(this)),
      m_delegate(new PlaylistCardDelegate(this)),
      m_list(new QListView(this)),
      m_empty(new StateCard(this))
{
    auto *heading = new QLabel(tr("Playlists"), this);
    heading->setObjectName(QStringLiteral("LibraryHeading"));
    auto *create = new QPushButton(tr("New Playlist"), this);
    create->setObjectName(QStringLiteral("NeutralButton"));
    create->setCursor(Qt::PointingHandCursor);

    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(kSidePadding, kToolbarPadY, kSidePadding, kToolbarPadY);
    toolbar->addWidget(heading);
    toolbar->addStretch();
    toolbar->addWidget(create);

    m_list->setObjectName(QStringLiteral("LibraryList"));
    m_list->setModel(m_model);
    m_list->setItemDelegate(m_delegate);
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->viewport()->installEventFilter(this);
    m_list->installEventFilter(this);

    m_empty->setContent(tr("No playlists yet."),
                        tr("Create a playlist, then add sheets from your library."));

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addLayout(toolbar);
    column->addWidget(m_empty);
    column->addWidget(m_list, 1);
    column->addStretch(0);

    connect(create, &QPushButton::clicked, this, &PlaylistsView::createRequested);
    connect(&m_playlists, &PlaylistService::changed, this, &PlaylistsView::rebuild);
    connect(&m_library, &LibraryService::changed, this, &PlaylistsView::rebuild);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, m_list->viewport(), [this] { m_list->viewport()->update(); });
    rebuild();
}

int PlaylistsView::sheetCount(const std::string &playlistId) const
{
    const Playlist *playlist = m_playlists.find(playlistId);
    if (!playlist) {
        return 0;
    }
    int count = 0;
    for (const std::string &key : playlist->sheetKeys) {
        if (m_library.indexOfPayload(key) >= 0) {
            ++count;
        }
    }
    return count;
}

void PlaylistsView::rebuild()
{
    m_model->clear();
    for (const Playlist &playlist : m_playlists.playlists()) {
        auto *item = new QStandardItem();
        item->setData(toQString(playlist.name), PlaylistCardDelegate::NameRole);
        item->setData(sheetCount(playlist.id), PlaylistCardDelegate::CountRole);
        item->setData(toQString(playlist.id), PlaylistCardDelegate::IdRole);
        item->setEditable(false);
        m_model->appendRow(item);
    }
    const bool empty = m_model->rowCount() == 0;
    m_empty->setVisible(empty);
    m_list->setVisible(!empty);
}

void PlaylistsView::setOpenMenuRow(int row)
{
    m_delegate->setOpenMenuRow(row);
    m_list->viewport()->update();
}

bool PlaylistsView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_list && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        const QModelIndex current = m_list->currentIndex();
        if (current.isValid() && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter || key->key() == Qt::Key_Space)) {
            emit playlistOpened(fromQString(current.data(PlaylistCardDelegate::IdRole).toString()));
            return true;
        }
        return false;
    }
    if (watched != m_list->viewport()) {
        return false;
    }
    if (event->type() == QEvent::MouseMove) {
        const QPointF point = static_cast<QMouseEvent *>(event)->position();
        const QModelIndex index = m_list->indexAt(point.toPoint());
        const QRect row = m_list->visualRect(index);
        m_delegate->setHover(index.isValid() ? index.row() : -1,
                             index.isValid() && PlaylistCardDelegate::actionRect(row).contains(point));
        m_list->viewport()->setCursor(index.isValid() && PlaylistCardDelegate::cardRect(row).contains(point)
                                          ? Qt::PointingHandCursor
                                          : Qt::ArrowCursor);
        m_list->viewport()->update();
    } else if (event->type() == QEvent::Leave) {
        m_delegate->setHover(-1, false);
        m_list->viewport()->update();
    } else if (event->type() == QEvent::MouseButtonRelease) {
        const QPointF point = static_cast<QMouseEvent *>(event)->position();
        const QModelIndex index = m_list->indexAt(point.toPoint());
        if (!index.isValid()) {
            return false;
        }
        const QRect row = m_list->visualRect(index);
        const std::string id = fromQString(index.data(PlaylistCardDelegate::IdRole).toString());
        const QRectF action = PlaylistCardDelegate::actionRect(row);
        if (action.contains(point)) {
            setOpenMenuRow(index.row());
            emit actionMenuRequested(id, QRect(m_list->viewport()->mapToGlobal(action.topLeft().toPoint()),
                                               action.size().toSize()));
            return true;
        }
        if (PlaylistCardDelegate::cardRect(row).contains(point)) {
            emit playlistOpened(id);
            return true;
        }
    }
    return false;
}

}  // namespace sky
