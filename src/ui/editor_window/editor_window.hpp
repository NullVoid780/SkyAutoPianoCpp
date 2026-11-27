#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include <QWidget>

#include "services/notifications/toast_service.hpp"
#include "ui/editor_window/editor_document.hpp"

class QBoxLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTimer;

namespace sky {

class ConfirmPopup;
class SheetAudio;
class EditorSetup;
class ElevationLayer;
class KeyPad;
class LibraryService;
class MetaPanel;
class SelectBox;
class StepGridView;
class TimelineView;
class ToolButton;
class UiStateService;
class ToastHost;
struct StoredSheet;

class EditorWindow final : public QWidget {
    Q_OBJECT

public:
    EditorWindow(LibraryService &library, UiStateService &uiState, std::filesystem::path soundFolder, QWidget *owner);
    ~EditorWindow() override;

    void openSheet(int libraryIndex);
    const std::string &payloadKey() const { return m_payloadKey; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    enum class Layout { Wide, Medium, Stacked };

    void fitToScreen(QWidget *owner);
    void applyResponsiveLayout();
    void elideTitle();
    void setDetailsOpen(bool open);
    int chromeHeight() const;
    QString selectionSummary() const;
    int libraryIndex() const;
    double sheetBpm() const;
    void load(int libraryIndex);
    void applyPayload(const StoredSheet &payload);
    void setGrid(const TimeGrid &grid);
    void moveCursorTo(int column, bool extend);
    std::pair<int, int> selection() const;
    void afterEdit();
    void toggleKey(int key, bool fromKeyboard);
    void toggleCell(int column, int key);
    void moveNote(qint64 time, int key, qint64 newTime, int newKey);
    void changeHold(qint64 time, int key, qint64 hold);
    void undoEdit();
    void redoEdit();
    void insertColumn();
    void deleteColumns();
    void eraseColumns();
    void copyColumns(bool cut);
    void pasteColumns(bool insert);
    void duplicateColumns();
    void shiftKeys(int delta);
    void snapToGrid();
    void moveSelection(int delta);
    void adjustHold(int deltaColumns);
    void dragColumn(int from, int to);
    void setAdvancedView(bool advanced);
    void applySetup(bool advanced, bool colorful);
    void pressColumn(int column, bool extend);
    void togglePlayback();
    void startPlayback(int column);
    void stopPlayback();
    void playbackTick();
    void toggleSound();
    void refreshSoundButton();
    void loadSounds();
    void refreshViews();
    void zoomViews(int steps);
    int pageColumns() const;
    void beginEditing(int field);
    void commitEditing();
    void cancelEditing();
    void saveNotes();
    void onSave();
    void onCancel();
    void exportSheet();
    void refreshActions();
    void refreshStatus();
    void refreshHeader();
    void refreshButtonIcons();
    void notify(ToastType type, const std::string &message, int durationMs = kDefaultToastMs);

    LibraryService &m_library;
    UiStateService &m_uiState;
    bool m_advanced = false;
    std::string m_payloadKey;
    EditorDocument m_document;
    TimeGrid m_grid;
    ColumnClip m_clip;
    bool m_loaded = false;
    bool m_steady = true;
    bool m_closeConfirmed = false;
    Layout m_layout = Layout::Wide;
    bool m_detailsOpen = true;
    bool m_detailsChosen = false;
    bool m_hintRoom = true;
    QString m_fullTitle;
    int m_cursor = -1;
    int m_anchor = -1;
    int m_editingField = -1;
    quint64 m_loadGeneration = 0;

    ToolButton *m_undo;
    ToolButton *m_redo;
    ToolButton *m_insert;
    ToolButton *m_delete;
    ToolButton *m_erase;
    ToolButton *m_copy;
    ToolButton *m_cut;
    ToolButton *m_paste;
    ToolButton *m_keyUp;
    ToolButton *m_keyDown;
    ToolButton *m_snap;
    ToolButton *m_earlier = nullptr;
    ToolButton *m_later = nullptr;
    ToolButton *m_holdLess = nullptr;
    ToolButton *m_holdMore = nullptr;
    ToolButton *m_simpleView = nullptr;
    ToolButton *m_advancedView = nullptr;
    StepGridView *m_steps = nullptr;
    QScrollArea *m_stepScroll = nullptr;
    QStackedWidget *m_views = nullptr;
    ToolButton *m_zoomOut;
    ToolButton *m_zoomIn;
    ToolButton *m_play = nullptr;
    ToolButton *m_sound = nullptr;
    std::unique_ptr<SheetAudio> m_audio;
    std::filesystem::path m_soundFolder;
    QTimer *m_playTimer = nullptr;
    QTimer *m_idleTimer = nullptr;
    bool m_playing = false;
    int m_playColumn = -1;
    SelectBox *m_gridSelect;
    TimelineView *m_timeline;
    QLabel *m_title;
    QLabel *m_subtitle;
    QLabel *m_saveState;
    QLabel *m_selectionTitle;
    QLabel *m_selectionDetail;
    QLabel *m_hint;
    QLabel *m_status;
    MetaPanel *m_meta;
    KeyPad *m_pad;
    QWidget *m_toolbar = nullptr;
    QWidget *m_side = nullptr;
    QWidget *m_info = nullptr;
    QWidget *m_songBox = nullptr;
    QPushButton *m_detailsToggle = nullptr;
    QBoxLayout *m_body = nullptr;
    QBoxLayout *m_sideLayout = nullptr;
    QPushButton *m_save;
    QPushButton *m_cancel;
    QPushButton *m_export;
    ElevationLayer *m_shadows;
    ConfirmPopup *m_confirm;
    EditorSetup *m_setup = nullptr;
    bool m_setupShown = false;
    ToastHost *m_toasts;
};

}  // namespace sky
