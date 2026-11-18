#pragma once

#include <string>

#include <QWidget>

class QListView;
class QStandardItemModel;

namespace sky {

class LibraryService;
class PlaylistCardDelegate;
class PlaylistService;
class StateCard;

class PlaylistsView final : public QWidget {
    Q_OBJECT

public:
    PlaylistsView(PlaylistService &playlists, LibraryService &library, QWidget *parent = nullptr);

    void setOpenMenuRow(int row);
    int sheetCount(const std::string &playlistId) const;

signals:
    void createRequested();
    void playlistOpened(const std::string &playlistId);
    void actionMenuRequested(const std::string &playlistId, const QRect &anchorGlobal);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuild();

    PlaylistService &m_playlists;
    LibraryService &m_library;
    QStandardItemModel *m_model;
    PlaylistCardDelegate *m_delegate;
    QListView *m_list;
    StateCard *m_empty;
};

}  // namespace sky
