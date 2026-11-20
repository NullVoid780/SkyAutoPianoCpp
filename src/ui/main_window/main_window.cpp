#include "ui/main_window/main_window.hpp"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <set>

#include "app/player_controller.hpp"
#include "app/version.hpp"
#include "core/string_utils.hpp"
#include "platform/file_dialog.hpp"
#include "platform/window_attributes.hpp"
#include "services/settings/config_service.hpp"
#include "services/library/favorites_service.hpp"
#include "services/library/library_service.hpp"
#include "services/library/playlist_service.hpp"
#include "services/library/recent_service.hpp"
#include "ui/core/elevation_layer.hpp"
#include "ui/main_window/add_sheet_button.hpp"
#include "ui/main_window/bulk_toolbar.hpp"
#include "ui/main_window/library_view.hpp"
#include "ui/main_window/nav_drawer.hpp"
#include "ui/main_window/nav_toggle.hpp"
#include "ui/main_window/player_footer.hpp"
#include "ui/main_window/playlist_detail_bar.hpp"
#include "ui/main_window/playlist_picker.hpp"
#include "ui/main_window/playlists_view.hpp"
#include "ui/main_window/scroll_top_button.hpp"
#include "ui/main_window/sheet_actions_menu.hpp"
#include "ui/main_window/store_view.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/confirm_popup.hpp"
#include "ui/widgets/icon_button.hpp"
#include "ui/widgets/search_field.hpp"
#include "ui/widgets/state_card.hpp"
#include "ui/widgets/toast_host.hpp"

namespace sky {

namespace {

constexpr int kSearchGap = 20;
constexpr int kSidePadding = 20;
constexpr int kHeaderPadY = 6;
constexpr int kHeaderGap = 8;
constexpr int kBulkEntryMargin = 10;
constexpr int kBulkToolbarGap = 12;
constexpr int kNavToggleInset = 16;
constexpr int kAddRight = 20;
constexpr int kAddBottom = 5;
constexpr int kScrollTopRight = 20;
constexpr int kScrollTopBottom = 170;

const QString kLocalPlaceholder = QStringLiteral("Search the music sheet or author name...");
const QString kStorePlaceholder = QStringLiteral("Search title, artist, or transcriber...");

QPushButton *makeBulkToggle(QWidget *parent)
{
    auto *button = new QPushButton(QStringLiteral("Multi-select"), parent);
    button->setObjectName(QStringLiteral("NeutralButton"));
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

}  // namespace

MainWindow::MainWindow(MainWindowServices services, QWidget *parent) : QWidget(parent), m_services(services)
{
    setObjectName(QStringLiteral("MainWindow"));
    setAttribute(Qt::WA_StyledBackground);
    setFocusPolicy(Qt::ClickFocus);
    setWindowTitle(QStringLiteral("Sky Auto Piano (Version: %1)").arg(QLatin1String(kAppVersion)));
    setWindowIcon(QIcon(QStringLiteral(":/icon/Icon9.ico")));
    resize(750, 600);
    setMinimumSize(700, 200);

    buildLayout();
    wireFooter();
    wireNavigation();
    applyConfig();
    refreshTarget();
    render();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        applyThemeTitleBar(this);
        refreshBulkToolbar();
        update();
    });
    applyThemeTitleBar(this);
}

void MainWindow::buildLayout()
{
    m_elevation = new ElevationLayer(this);

    auto *content = new QWidget(this);
    content->setObjectName(QStringLiteral("LibraryArea"));
    auto *column = new QVBoxLayout(content);
    column->setContentsMargins(0, kNavToggleInset, 0, PlayerFooter::kHeight - PlayerFooter::kOverhang);
    column->setSpacing(0);

    m_search = new SearchField(content);
    auto *searchRow = new QHBoxLayout();
    searchRow->addStretch();
    searchRow->addWidget(m_search);
    searchRow->addStretch();
    column->addLayout(searchRow);
    column->addSpacing(kSearchGap);

    m_pages = new QStackedWidget(content);
    column->addWidget(m_pages, 1);

    m_localPage = new QWidget(m_pages);
    auto *local = new QVBoxLayout(m_localPage);
    local->setContentsMargins(0, 0, 0, 0);
    local->setSpacing(0);

    m_headingRow = new QWidget(m_localPage);
    auto *heading = new QHBoxLayout(m_headingRow);
    heading->setContentsMargins(kSidePadding, kHeaderPadY, kSidePadding, kHeaderPadY);
    m_heading = new QLabel(m_headingRow);
    m_heading->setObjectName(QStringLiteral("LibraryHeading"));
    m_bulkToggle = makeBulkToggle(m_headingRow);
    heading->addWidget(m_heading);
    heading->addStretch();
    heading->addWidget(m_bulkToggle);
    local->addWidget(m_headingRow);

    m_detailBar = new PlaylistDetailBar(m_localPage);
    local->addWidget(m_detailBar);
    m_bulkEntryRow = new QWidget(m_localPage);
    auto *entry = new QHBoxLayout(m_bulkEntryRow);
    entry->setContentsMargins(kSidePadding, kBulkEntryMargin, kSidePadding, kBulkEntryMargin);
    m_bulkEntryToggle = makeBulkToggle(m_bulkEntryRow);
    entry->addStretch();
    entry->addWidget(m_bulkEntryToggle);
    local->addWidget(m_bulkEntryRow);
    local->addSpacing(kHeaderGap);

    auto *toolbarRow = new QHBoxLayout();
    toolbarRow->setContentsMargins(kSidePadding, 0, kSidePadding, kBulkToolbarGap);
    m_bulkToolbar = new BulkToolbar(m_localPage);
    toolbarRow->addWidget(m_bulkToolbar);
    local->addLayout(toolbarRow);

    m_model = new LibraryModel(m_services.library, m_services.favorites, m_services.recent, this);
    m_empty = new StateCard(m_localPage);
    m_empty->setContent(QStringLiteral("No sheets found."),
                        QStringLiteral("Import a local sheet with the plus button, or switch tabs."));
    m_list = new LibraryView(m_model, m_localPage);
    local->addWidget(m_empty);
    local->addWidget(m_list, 1);
    local->addStretch(0);
    m_pages->addWidget(m_localPage);

    m_playlistsView = new PlaylistsView(m_services.playlists, m_services.library, m_pages);
    m_pages->addWidget(m_playlistsView);
    m_storeView = new StoreView(m_services.store, m_services.library, m_services.player, m_pages);
    m_pages->addWidget(m_storeView);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(content);

    m_footer = new PlayerFooter(this);
    m_addButton = createAddSheetButton(this);
    m_scrollTop = createScrollTopButton(this);
    m_navToggle = createNavToggle(this);
    m_drawer = new NavDrawer(this);
    m_drawer->setToggle(m_navToggle);
    m_menu = new SheetActionsMenu(this);
    m_picker = new PlaylistPicker(m_services.playlists, m_services.library, this);
    m_confirm = new ConfirmPopup(this);
    m_toasts = new ToastHost(this);

    m_elevation->track(m_search, 10, [this] {
        if (!m_search->hasFocus()) {
            return std::vector<ShadowSpec>{};
        }
        return std::vector<ShadowSpec>{ShadowSpec{5, {0, 0}, currentTheme().hoverColor}};
    });
    m_elevation->track(m_footer, 0, [] { return std::vector<ShadowSpec>{currentTheme().shadowFooter}; });
    m_elevation->track(m_scrollTop, 8, [] { return std::vector<ShadowSpec>{currentTheme().shadowControl}; });
    m_elevation->track(m_bulkToolbar, 14, [] { return std::vector<ShadowSpec>{currentTheme().shadowBulk}; });
    for (QPushButton *toggle : {m_bulkToggle, m_bulkEntryToggle}) {
        m_elevation->track(toggle, 10, [toggle] {
            return toggle->isEnabled() ? std::vector<ShadowSpec>{currentTheme().shadowControl} : std::vector<ShadowSpec>{};
        });
    }

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (m_tab == LibraryTab::Store) {
            m_storeView->setQuery(text);
        } else {
            m_localQuery = text;
            m_model->setSearchTerm(text);
        }
    });
    connect(m_model, &QAbstractItemModel::modelReset, this, &MainWindow::refreshEmptyState);
    connect(m_model, &LibraryModel::selectionChanged, this, &MainWindow::refreshBulkToolbar);
    connect(m_list, &LibraryView::cardActivated, this, &MainWindow::onCardActivated);
    connect(m_list, &LibraryView::actionMenuRequested, this, &MainWindow::openSheetMenu);
    connect(m_list, &LibraryView::scrolled, this, [this](int offset) {
        m_scrollTop->setVisible(offset >= kScrollTopThreshold && showingLocalList());
        m_menu->hide();
    });
    connect(m_scrollTop, &QAbstractButton::clicked, m_list, &LibraryView::scrollToTopSmoothly);
    connect(m_addButton, &QAbstractButton::clicked, this, &MainWindow::importSheets);
    connect(m_menu, &SheetActionsMenu::triggered, this, [this](SheetAction action) { handleMenuAction(static_cast<int>(action)); });
    connect(m_menu, &SheetActionsMenu::closed, this, [this] {
        m_list->setOpenMenuRow(-1);
        m_playlistsView->setOpenMenuRow(-1);
    });

    for (QPushButton *toggle : {m_bulkToggle, m_bulkEntryToggle}) {
        connect(toggle, &QPushButton::clicked, this, [this] { setBulkMode(!m_model->bulkMode()); });
    }
    connect(m_bulkToolbar, &BulkToolbar::doneRequested, this, [this] { setBulkMode(false); });
    connect(m_bulkToolbar, &BulkToolbar::playlistChosen, this, [this](const std::string &id) { m_bulkPlaylistId = id; });
    connect(m_bulkToolbar, &BulkToolbar::selectAllRequested, this, [this] {
        const std::vector<std::string> visible = m_model->visibleKeys();
        std::set<std::string> selected = m_model->selected();
        const bool all = !visible.empty() &&
                         std::all_of(visible.begin(), visible.end(), [&](const std::string &key) { return selected.contains(key); });
        for (const std::string &key : visible) {
            if (all) {
                selected.erase(key);
            } else {
                selected.insert(key);
            }
        }
        m_model->setSelected(selected);
    });
    connect(m_bulkToolbar, &BulkToolbar::addToPlaylistRequested, this, &MainWindow::addSelectionToPlaylist);
    connect(m_bulkToolbar, &BulkToolbar::deleteRequested, this, &MainWindow::deleteSelection);

    connect(m_detailBar, &PlaylistDetailBar::backRequested, this, [this] {
        m_activePlaylistId.clear();
        render();
    });
    connect(m_playlistsView, &PlaylistsView::createRequested, m_picker, &PlaylistPicker::openForNew);
    connect(m_playlistsView, &PlaylistsView::playlistOpened, this, &MainWindow::openPlaylist);
    connect(m_playlistsView, &PlaylistsView::actionMenuRequested, this, &MainWindow::openPlaylistMenu);
    connect(m_picker, &PlaylistPicker::toastRequested, this, &MainWindow::showToast);
    connect(m_picker, &Overlay::opened, this, [this] { emit shortcutsSuspended(true); });
    connect(m_picker, &Overlay::closed, this, [this] { emit shortcutsSuspended(false); });
    connect(m_storeView, &StoreView::toastRequested, this, &MainWindow::showToast);
    connect(&m_services.playlists, &PlaylistService::changed, this, [this] {
        if (m_tab == LibraryTab::Playlists && !m_activePlaylistId.empty()) {
            render();
        }
        refreshBulkToolbar();
    });
    connect(&m_services.library, &LibraryService::changed, this, [this] {
        if (m_tab == LibraryTab::Playlists && !m_activePlaylistId.empty()) {
            render();
        }
    });

    m_services.player.setVisibleIndicesProvider([this] {
        return showingLocalList() ? m_model->visibleIndices() : std::vector<int>{};
    });
    connect(&m_services.player, &PlayerController::targetChanged, this, &MainWindow::refreshTarget);
    connect(&m_services.player, &PlayerController::positionChanged, m_footer, &PlayerFooter::setPosition);
    connect(&m_services.player, &PlayerController::playingChanged, m_footer, &PlayerFooter::setPlaying);
    connect(&m_services.player, &PlayerController::loopModeChanged, m_footer, &PlayerFooter::setLoopMode);
    connect(&m_services.config, &ConfigService::changed, this, &MainWindow::applyConfig);
}

void MainWindow::wireNavigation()
{
    connect(m_navToggle, &QAbstractButton::clicked, this, [this] { m_drawer->setOpen(!m_drawer->isOpen()); });
    connect(m_drawer, &NavDrawer::tabChosen, this, [this](LibraryTab tab) {
        setActiveTab(tab);
        setFocus();
    });
    connect(m_drawer, &NavDrawer::openChanged, this, [this](bool open) {
        const QString label = open ? QStringLiteral("Close navigation") : QStringLiteral("Open navigation");
        m_navToggle->setToolTip(label);
        m_navToggle->setAccessibleName(label);
    });
}

void MainWindow::wireFooter()
{
    PlayerController &player = m_services.player;
    connect(m_footer, &PlayerFooter::settingsRequested, this, &MainWindow::settingsRequested);
    connect(m_footer, &PlayerFooter::themeToggleRequested, this, &MainWindow::themeToggleRequested);
    connect(m_footer, &PlayerFooter::previousRequested, &player, &PlayerController::previous);
    connect(m_footer, &PlayerFooter::playRequested, &player, &PlayerController::togglePlay);
    connect(m_footer, &PlayerFooter::nextRequested, &player, &PlayerController::next);
    connect(m_footer, &PlayerFooter::practiceModeChosen, &player,
            [&player](bool practice) { player.setPlayMode(practice ? PlayMode::Practice : PlayMode::Auto); });
    connect(&player, &PlayerController::playModeChanged, m_footer,
            [this](PlayMode mode) { m_footer->setPracticeMode(mode == PlayMode::Practice); });
    connect(m_footer, &PlayerFooter::seekRequested, &player, &PlayerController::seek);
    connect(m_footer, &PlayerFooter::loopCycleRequested, &player, &PlayerController::cycleLoopMode);
    connect(m_footer, &PlayerFooter::longPressToggled, this, [this](bool enabled) {
        OrderedJson partial = OrderedJson::object();
        partial["longPressMode"] = enabled;
        m_services.config.updatePanel(partial);
    });
    connect(m_footer, &PlayerFooter::speedCommitted, this, [this](double speed) {
        OrderedJson partial = OrderedJson::object();
        partial["speed"] = jsonNumber(speed);
        m_services.config.updatePanel(partial);
    });
    connect(m_footer, &PlayerFooter::delayChanged, this, [this](int seconds) {
        OrderedJson partial = OrderedJson::object();
        partial["delayNext"] = seconds;
        m_services.config.updatePanel(partial);
    });
}

void MainWindow::setActiveTab(LibraryTab tab)
{
    if (m_tab == LibraryTab::Store) {
        m_storeView->setQuery(m_search->text());
    } else {
        m_localQuery = m_search->text();
    }
    if (m_tab != tab && m_model->bulkMode()) {
        setBulkMode(false);
    }
    m_tab = tab;
    m_drawer->setActiveTab(tab);
    const bool store = tab == LibraryTab::Store;
    m_addButton->setVisible(!store);
    {
        const QSignalBlocker blocker(m_search);
        m_search->setPlaceholderText(store ? kStorePlaceholder : kLocalPlaceholder);
        m_search->setText(store ? m_storeView->query() : m_localQuery);
    }
    if (store) {
        m_storeView->activate();
    } else {
        m_model->setSearchTerm(m_localQuery);
    }
    render();
}

bool MainWindow::showingLocalList() const
{
    return m_pages->currentWidget() == m_localPage;
}

std::vector<int> MainWindow::playlistSheetIndices(const std::string &playlistId) const
{
    std::vector<int> indices;
    if (const Playlist *playlist = m_services.playlists.find(playlistId)) {
        for (const std::string &key : playlist->sheetKeys) {
            const int index = m_services.library.indexOfPayload(key);
            if (index >= 0) {
                indices.push_back(index);
            }
        }
    }
    return indices;
}

void MainWindow::openPlaylist(const std::string &playlistId)
{
    if (!m_services.playlists.find(playlistId)) {
        return;
    }
    m_activePlaylistId = playlistId;
    render();
}

void MainWindow::render()
{
    if (m_tab == LibraryTab::Store) {
        m_pages->setCurrentWidget(m_storeView);
    } else if (m_tab == LibraryTab::Playlists && m_services.playlists.find(m_activePlaylistId) == nullptr) {
        m_activePlaylistId.clear();
        m_pages->setCurrentWidget(m_playlistsView);
    } else {
        const bool detail = m_tab == LibraryTab::Playlists;
        m_headingRow->setVisible(!detail);
        m_detailBar->setVisible(detail);
        m_bulkEntryRow->setVisible(detail);
        m_heading->setText(LibraryModel::tabTitle(m_tab));
        if (detail) {
            const Playlist *playlist = m_services.playlists.find(m_activePlaylistId);
            const std::vector<int> indices = playlistSheetIndices(m_activePlaylistId);
            m_detailBar->setPlaylist(toQString(playlist->name), static_cast<int>(indices.size()));
            m_model->setTab(LibraryTab::Playlists);
            m_model->setRestriction(indices, true);
        } else {
            m_model->setTab(m_tab);
            m_model->setRestriction({}, false);
        }
        m_pages->setCurrentWidget(m_localPage);
    }
    m_scrollTop->setVisible(showingLocalList() && m_list->verticalScrollBar()->value() >= kScrollTopThreshold);
    refreshEmptyState();
    refreshBulkToolbar();
}

void MainWindow::refreshEmptyState()
{
    const bool empty = m_model->rowCount() == 0;
    m_empty->setVisible(empty);
    m_list->setVisible(!empty);
}

void MainWindow::refreshBulkToolbar()
{
    const bool bulk = m_model->bulkMode();
    m_bulkToolbar->setVisible(bulk && showingLocalList());
    const QString label = bulk ? QStringLiteral("Exit multi-select") : QStringLiteral("Multi-select");
    for (QPushButton *toggle : {m_bulkToggle, m_bulkEntryToggle}) {
        const QSignalBlocker blocker(toggle);
        toggle->setText(label);
        toggle->setChecked(bulk);
    }
    if (!bulk) {
        return;
    }
    const std::vector<std::string> visible = m_model->visibleKeys();
    const std::set<std::string> &selected = m_model->selected();
    const bool all = !visible.empty() &&
                     std::all_of(visible.begin(), visible.end(), [&](const std::string &key) { return selected.contains(key); });
    m_bulkToolbar->setPlaylists(m_services.playlists.playlists(), m_bulkPlaylistId);
    m_bulkToolbar->setState(static_cast<int>(selected.size()), all, !visible.empty());
}

void MainWindow::setBulkMode(bool enabled)
{
    if (enabled) {
        const auto &playlists = m_services.playlists.playlists();
        if (!m_services.playlists.find(m_bulkPlaylistId)) {
            m_bulkPlaylistId = playlists.empty() ? std::string() : playlists.front().id;
        }
    } else {
        m_bulkPlaylistId.clear();
    }
    m_menu->hide();
    m_model->setBulkMode(enabled);
    refreshBulkToolbar();
}

void MainWindow::onCardActivated(int libraryIndex)
{
    const SheetEntry *entry = m_services.library.entry(libraryIndex);
    if (!entry) {
        return;
    }
    if (m_model->bulkMode()) {
        m_model->toggleSelected(entry->payloadFile());
        return;
    }
    m_services.player.selectLocal(libraryIndex, true);
}

void MainWindow::addSelectionToPlaylist()
{
    const Playlist *playlist = m_services.playlists.find(m_bulkPlaylistId);
    if (!playlist) {
        showToast(Toast{ToastType::Warning, {}, "Create or select a playlist first.", kDefaultToastMs});
        return;
    }
    std::vector<std::string> keys;
    for (const SheetEntry &entry : m_services.library.entries()) {
        if (m_model->selected().contains(entry.payloadFile())) {
            keys.push_back(entry.payloadFile());
        }
    }
    if (keys.empty()) {
        return;
    }
    const std::string name = playlist->name;
    auto added = m_services.playlists.addSheets(m_bulkPlaylistId, keys);
    if (!added) {
        showToast(Toast{ToastType::Warning, {}, added.error().userMessage, kDefaultToastMs});
        return;
    }
    showToast(Toast{added.value() > 0 ? ToastType::Success : ToastType::Warning, {},
                    PlaylistService::bulkAddMessage(added.value(), name), kDefaultToastMs});
}

void MainWindow::deleteSelection()
{
    const std::set<std::string> selected = m_model->selected();
    if (selected.empty()) {
        return;
    }
    m_confirm->ask(toQString(LibraryService::deleteConfirmation(selected.size())), [this, selected] {
        const DeleteSummary summary = m_services.library.deleteSheets(selected);
        showToast(LibraryService::deleteToast(summary));
        if (!summary.deletedKeys.empty()) {
            setBulkMode(false);
        }
    });
}

void MainWindow::setSkyStatus(const QString &text, int tone, const QString &tip)
{
    m_footer->setSkyStatus(text, tone, tip);
}

void MainWindow::openSheetMenu(int libraryIndex, const QRect &anchor)
{
    const SheetEntry *entry = m_services.library.entry(libraryIndex);
    if (!entry) {
        return;
    }
    m_menuContext = MenuContext::Sheet;
    m_menuLibraryIndex = libraryIndex;
    const bool favorite = m_services.favorites.isFavorite(entry->name());
    std::vector<SheetActionEntry> entries{
        {SheetAction::Favorite, favorite ? QStringLiteral("Unfavorite") : QStringLiteral("Favorite"), QStringLiteral("heart"),
         true, false, favorite},
        {SheetAction::Edit, QStringLiteral("Edit"), QStringLiteral("sheet-editor")},
        {SheetAction::Practice, QStringLiteral("Play yourself"), QStringLiteral("music-note-beamed")},
        {SheetAction::AddToPlaylist, QStringLiteral("Add to playlist"), QStringLiteral("collection-plus")},
    };
    if (m_tab == LibraryTab::Playlists && !m_activePlaylistId.empty()) {
        const Playlist *playlist = m_services.playlists.find(m_activePlaylistId);
        const auto &keys = playlist->sheetKeys;
        const auto position = std::find(keys.begin(), keys.end(), entry->payloadFile());
        if (position != keys.end()) {
            entries.push_back({SheetAction::MoveUp, QStringLiteral("Move up"), QStringLiteral("arrow-up-short"),
                               position != keys.begin()});
            entries.push_back({SheetAction::MoveDown, QStringLiteral("Move down"), QStringLiteral("arrow-down-short"),
                               position + 1 != keys.end()});
        }
        entries.push_back({SheetAction::RemoveFromPlaylist, QStringLiteral("Remove from playlist"), QStringLiteral("dash-circle")});
    }
    entries.push_back({SheetAction::Delete, QStringLiteral("Delete"), QStringLiteral("trash3"), true, true});
    m_menu->setEntries(std::move(entries));
    m_menu->popupBeside(anchor, QRect(mapToGlobal(QPoint(0, 0)), size()));
}

void MainWindow::openPlaylistMenu(const std::string &playlistId, const QRect &anchor)
{
    m_menuContext = MenuContext::Playlist;
    m_menuPlaylistId = playlistId;
    m_menu->setEntries({
        {SheetAction::Rename, QStringLiteral("Rename"), QStringLiteral("pencil")},
        {SheetAction::Delete, QStringLiteral("Delete"), QStringLiteral("trash3"), true, true},
    });
    m_menu->popupBeside(anchor, QRect(mapToGlobal(QPoint(0, 0)), size()));
}

void MainWindow::handleMenuAction(int rawAction)
{
    const auto action = static_cast<SheetAction>(rawAction);
    if (m_menuContext == MenuContext::Playlist) {
        const std::string id = m_menuPlaylistId;
        if (action == SheetAction::Rename) {
            m_picker->openForRename(id);
        } else if (action == SheetAction::Delete) {
            m_confirm->ask(QStringLiteral("Delete this playlist? Sheets will remain in your library."), [this, id] {
                m_services.playlists.remove(id);
                if (m_activePlaylistId == id) {
                    m_activePlaylistId.clear();
                }
                render();
            });
        }
        return;
    }

    const SheetEntry *entry = m_services.library.entry(m_menuLibraryIndex);
    if (!entry) {
        return;
    }
    const std::string key = entry->payloadFile();
    switch (action) {
    case SheetAction::Favorite:
        m_services.favorites.toggle(entry->name());
        break;
    case SheetAction::Edit:
        emit editorRequested(m_menuLibraryIndex);
        break;
    case SheetAction::Practice:
        emit practiceRequested(m_menuLibraryIndex);
        break;
    case SheetAction::AddToPlaylist:
        m_picker->openForSheet(m_menuLibraryIndex);
        break;
    case SheetAction::MoveUp:
        m_services.playlists.moveSheet(m_activePlaylistId, key, -1);
        break;
    case SheetAction::MoveDown:
        m_services.playlists.moveSheet(m_activePlaylistId, key, 1);
        break;
    case SheetAction::RemoveFromPlaylist:
        m_services.playlists.removeSheet(m_activePlaylistId, key);
        break;
    case SheetAction::Delete:
        if (auto removed = m_services.library.deleteSheet(m_menuLibraryIndex); !removed) {
            showToast(Toast{ToastType::Error, {}, removed.error().userMessage, kDefaultToastMs});
        }
        break;
    default:
        break;
    }
}

void MainWindow::applyConfig()
{
    const Config config = m_services.config.value();
    m_footer->setShortcutLabels(toQString(config.shortcut.pre), toQString(config.shortcut.play),
                                toQString(config.shortcut.next));
    if (!m_footer->isEditingSpeed()) {
        m_footer->setSpeed(config.panel.speed);
    }
    m_footer->setLongPress(config.panel.longPressMode);
    const double delay = std::isfinite(config.panel.delayNext) ? config.panel.delayNext : 1.0;
    m_footer->setDelaySeconds(static_cast<int>(std::clamp(delay, 0.0, 5.0)));
}

void MainWindow::refreshTarget()
{
    const PlaybackTarget &target = m_services.player.target();
    if (target.kind == PlaybackTarget::Kind::None) {
        m_footer->showEmpty();
        return;
    }
    m_footer->showTarget(toQString(target.name), target.hasHoldNotes, target.totalSeconds);
}

void MainWindow::importSheets()
{
    const std::vector<std::filesystem::path> files = chooseSheetFiles(this);
    if (!files.empty()) {
        m_services.library.importFiles(files);
    }
}

void MainWindow::confirm(const QString &message, std::function<void()> onConfirm)
{
    m_confirm->ask(message, std::move(onConfirm));
}

void MainWindow::showToast(const Toast &toast)
{
    m_toasts->show(toast);
}

bool MainWindow::textInputHasFocus() const
{
    const QWidget *focus = QApplication::focusWidget();
    return qobject_cast<const QLineEdit *>(focus) || qobject_cast<const QAbstractSpinBox *>(focus) ||
           qobject_cast<const QTextEdit *>(focus) || qobject_cast<const QPlainTextEdit *>(focus) ||
           qobject_cast<const QComboBox *>(focus);
}

void MainWindow::handleShortcut(ShortcutAction action)
{
    if (textInputHasFocus() || m_picker->isOpen()) {
        return;
    }
    PlayerController &player = m_services.player;
    switch (action) {
    case ShortcutAction::Previous:
        player.previous();
        break;
    case ShortcutAction::PlayPause:
        player.togglePlay();
        break;
    case ShortcutAction::Next:
        player.next();
        break;
    case ShortcutAction::IncreaseSpeed:
    case ShortcutAction::DecreaseSpeed:
        m_footer->setSpeed(m_services.config.panel().speed);
        break;
    }
}

void MainWindow::bringToFront()
{
    if (isMinimized()) {
        showNormal();
    }
    show();
    raise();
    activateWindow();
}

void MainWindow::alignSearchWithNavToggle()
{
    QLayout *column = m_search->parentWidget()->layout();
    const int top = std::max(0, kNavToggleInset + (m_navToggle->height() - m_search->sizeHint().height()) / 2);
    const QMargins margins = column->contentsMargins();
    if (margins.top() != top) {
        column->setContentsMargins(margins.left(), top, margins.right(), margins.bottom());
    }
}

void MainWindow::positionOverlays()
{
    m_footer->setGeometry(0, height() - PlayerFooter::kHeight + PlayerFooter::kOverhang, width(), PlayerFooter::kHeight);
    m_addButton->move(width() - kAddRight - m_addButton->width(), height() - kAddBottom - m_addButton->height());
    m_scrollTop->move(width() - kScrollTopRight - m_scrollTop->width(), height() - kScrollTopBottom - m_scrollTop->height());
    m_navToggle->move(kNavToggleInset, kNavToggleInset);
    alignSearchWithNavToggle();
    m_drawer->syncToWindow();
    m_footer->raise();
    m_scrollTop->raise();
    m_addButton->raise();
    m_drawer->raise();
    m_navToggle->raise();
    m_toasts->relayout();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_menu->hide();
    positionOverlays();
}

}  // namespace sky
