#pragma once

#include <set>
#include <string>
#include <vector>

#include <QAbstractListModel>
#include <QString>

namespace sky {

class FavoritesService;
class LibraryService;
class RecentService;

enum class LibraryTab { AllSongs, Favorite, Playlists, RecentPlay, Store };

class LibraryModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        AuthorRole,
        TranscriberRole,
        BpmRole,
        HoldRole,
        LibraryIndexRole,
        FavoriteRole,
        PayloadKeyRole,
        SelectedRole,
        BulkModeRole,
    };

    LibraryModel(LibraryService &library, FavoritesService &favorites, RecentService &recent, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    void setTab(LibraryTab tab);
    void setSearchTerm(const QString &term);
    void setRestriction(std::vector<int> indices, bool restricted);
    LibraryTab tab() const { return m_tab; }
    int libraryIndex(int row) const;
    int rowForLibraryIndex(int libraryIndex) const;
    const std::vector<int> &visibleIndices() const { return m_rows; }

    void setBulkMode(bool enabled);
    bool bulkMode() const { return m_bulkMode; }
    void toggleSelected(const std::string &payloadKey);
    void setSelected(const std::set<std::string> &keys);
    const std::set<std::string> &selected() const { return m_selected; }
    std::vector<std::string> visibleKeys() const;

    static QString tabTitle(LibraryTab tab);

signals:
    void selectionChanged();

public slots:
    void refresh();

private:
    bool matches(int libraryIndex) const;

    LibraryService &m_library;
    FavoritesService &m_favorites;
    RecentService &m_recent;
    LibraryTab m_tab = LibraryTab::AllSongs;
    QString m_search;
    std::vector<int> m_restriction;
    bool m_restricted = false;
    std::vector<int> m_rows;
    std::set<std::string> m_selected;
    bool m_bulkMode = false;
};

}  // namespace sky
