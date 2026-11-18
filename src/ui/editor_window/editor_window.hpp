#pragma once

#include <memory>
#include <string>

#include <QWidget>

#include "model/note.hpp"
#include "services/notifications/toast_service.hpp"
#include "ui/editor_window/editor_document.hpp"

class QPushButton;
class QScrollArea;

namespace sky {

class ConfirmPopup;
class ElevationLayer;
class KeyPad;
class LibraryService;
class MetaPanel;
class NoteGrid;
class ToastHost;
struct StoredSheet;

class EditorWindow final : public QWidget {
    Q_OBJECT

public:
    EditorWindow(LibraryService &library, QWidget *owner);

    void openSheet(int libraryIndex);
    const std::string &payloadKey() const { return m_payloadKey; }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    int libraryIndex() const;
    void load(int libraryIndex);
    void applyPayload(const StoredSheet &payload);
    void selectCell(int cell);
    void clearSelection();
    void toggleKey(int keyIndex, bool fromKeyboard);
    void beginEditing(int field);
    void commitEditing();
    void cancelEditing();
    void saveGrid();
    void onSave();
    void onCancel();
    void exportSheet();
    void refreshActions();
    void notify(ToastType type, const std::string &message, int durationMs = kDefaultToastMs);
    void positionBottomBar();

    LibraryService &m_library;
    std::string m_payloadKey;
    EditorDocument m_document;
    std::vector<SongNote> m_payloadNotes;
    bool m_loaded = false;
    bool m_hold = false;
    int m_editingField = -1;
    quint64 m_loadGeneration = 0;

    QScrollArea *m_scroll;
    NoteGrid *m_grid;
    QWidget *m_bottomBar;
    MetaPanel *m_meta;
    KeyPad *m_pad;
    QWidget *m_actions;
    QPushButton *m_save;
    QPushButton *m_cancel;
    QPushButton *m_export;
    ElevationLayer *m_shadows;
    ConfirmPopup *m_confirm;
    ToastHost *m_toasts;
};

}  // namespace sky
