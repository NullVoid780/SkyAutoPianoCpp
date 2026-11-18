#pragma once

#include <functional>
#include <string>

#include <QWidget>

#include "services/settings/shortcut_service.hpp"
#include "services/notifications/toast_service.hpp"
#include "ui/main_window/library_model.hpp"

class QLabel;
class QPushButton;
class QStackedWidget;

namespace sky {

class BulkToolbar;
class ConfigService;
class ConfirmPopup;
class ElevationLayer;
class FavoritesService;
class IconButton;
class LibraryService;
class LibraryView;
class NavDrawer;
class PlayerController;
class PlayerFooter;
class PlaylistDetailBar;
class PlaylistPicker;
class PlaylistService;
class PlaylistsView;
class RecentService;
class SearchField;
class SheetActionsMenu;
class StateCard;
class StoreService;
class StoreView;
class ToastHost;

struct MainWindowServices {
    LibraryService &library;
    FavoritesService &favorites;
    RecentService &recent;
    PlaylistService &playlists;
    StoreService &store;
    ConfigService &config;
    PlayerController &player;
};

class MainWindow final : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(MainWindowServices services, QWidget *parent = nullptr);

    void showToast(const Toast &toast);
    void confirm(const QString &message, std::function<void()> onConfirm);
    void handleShortcut(ShortcutAction action);
    void applyConfig();
    void bringToFront();

signals:
    void settingsRequested();
    void themeToggleRequested();
    void editorRequested(int libraryIndex);
    void shortcutsSuspended(bool suspended);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class MenuContext { Sheet, Playlist };

    void buildLayout();
    void wireFooter();
    void wireNavigation();
    void setActiveTab(LibraryTab tab);
    void render();
    void refreshEmptyState();
    void refreshTarget();
    void refreshBulkToolbar();
    void setBulkMode(bool enabled);
    void openPlaylist(const std::string &playlistId);
    std::vector<int> playlistSheetIndices(const std::string &playlistId) const;
    bool showingLocalList() const;
    void onCardActivated(int libraryIndex);
    void openSheetMenu(int libraryIndex, const QRect &anchor);
    void openPlaylistMenu(const std::string &playlistId, const QRect &anchor);
    void handleMenuAction(int action);
    void addSelectionToPlaylist();
    void deleteSelection();
    void importSheets();
    void positionOverlays();
    void alignSearchWithNavToggle();
    bool textInputHasFocus() const;

    MainWindowServices m_services;

    ElevationLayer *m_elevation = nullptr;
    SearchField *m_search = nullptr;
    QStackedWidget *m_pages = nullptr;
    QWidget *m_localPage = nullptr;
    QWidget *m_headingRow = nullptr;
    QLabel *m_heading = nullptr;
    PlaylistDetailBar *m_detailBar = nullptr;
    QWidget *m_bulkEntryRow = nullptr;
    QPushButton *m_bulkToggle = nullptr;
    QPushButton *m_bulkEntryToggle = nullptr;
    BulkToolbar *m_bulkToolbar = nullptr;
    LibraryModel *m_model = nullptr;
    StateCard *m_empty = nullptr;
    LibraryView *m_list = nullptr;
    PlaylistsView *m_playlistsView = nullptr;
    StoreView *m_storeView = nullptr;
    PlayerFooter *m_footer = nullptr;
    IconButton *m_navToggle = nullptr;
    NavDrawer *m_drawer = nullptr;
    IconButton *m_addButton = nullptr;
    IconButton *m_scrollTop = nullptr;
    SheetActionsMenu *m_menu = nullptr;
    PlaylistPicker *m_picker = nullptr;
    ConfirmPopup *m_confirm = nullptr;
    ToastHost *m_toasts = nullptr;

    LibraryTab m_tab = LibraryTab::AllSongs;
    std::string m_activePlaylistId;
    QString m_localQuery;
    std::string m_bulkPlaylistId;
    MenuContext m_menuContext = MenuContext::Sheet;
    int m_menuLibraryIndex = -1;
    std::string m_menuPlaylistId;
};

}  // namespace sky
