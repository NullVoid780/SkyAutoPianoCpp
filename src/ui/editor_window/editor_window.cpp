#include "ui/editor_window/editor_window.hpp"

#include <QKeyEvent>
#include <QLayout>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QThreadPool>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

#include "codec/sheet_exporter.hpp"
#include "core/logger.hpp"
#include "core/string_utils.hpp"
#include "platform/file_dialog.hpp"
#include "platform/window_attributes.hpp"
#include "services/library/library_service.hpp"
#include "ui/core/elevation_layer.hpp"
#include "ui/editor_window/editable_field.hpp"
#include "ui/editor_window/key_pad.hpp"
#include "ui/editor_window/meta_panel.hpp"
#include "ui/editor_window/note_grid.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/confirm_popup.hpp"
#include "ui/widgets/toast_host.hpp"

namespace sky {

namespace {

constexpr int kWidth = 900;
constexpr int kHeight = 600;
constexpr int kPadding = 15;
constexpr int kGridBottom = 250;
constexpr int kGridGap = 10;
constexpr int kBarTop = 20;
constexpr int kBarBottom = 10;
constexpr int kMetaLeft = 20;
constexpr int kMetaBottom = 25;
constexpr int kActionsOffset = 72;
constexpr int kActionGap = 8;
constexpr int kActionWidth = 100;
constexpr int kExportBottom = 47;
constexpr double kExportRight = 78.5;
constexpr int kSavedToastMs = 2000;
constexpr int kErrorToastMs = 3000;

constexpr std::array<const char *, 4> kFieldKeys = {"name", "author", "transcribedBy", "bpm"};

const QString kHoldTooltip = QStringLiteral("Hold sheets are read-only in the current editor.");

std::optional<long long> parseLeadingInt(const QString &text)
{
    const std::string raw = fromQString(text.trimmed());
    std::size_t pos = 0;
    bool negative = false;
    if (pos < raw.size() && (raw[pos] == '+' || raw[pos] == '-')) {
        negative = raw[pos] == '-';
        ++pos;
    }
    long long value = 0;
    bool digits = false;
    while (pos < raw.size() && std::isdigit(static_cast<unsigned char>(raw[pos])) && value < 1000000000LL) {
        value = value * 10 + (raw[pos] - '0');
        digits = true;
        ++pos;
    }
    if (!digits) {
        return std::nullopt;
    }
    return negative ? -value : value;
}

QPushButton *makeButton(const QString &text, const QString &objectName, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::TabFocus);
    return button;
}

}  // namespace

EditorWindow::EditorWindow(LibraryService &library, QWidget *owner)
    : QWidget(owner, Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint |
                         Qt::WindowCloseButtonHint),
      m_library(library),
      m_scroll(new QScrollArea(this)),
      m_grid(new NoteGrid()),
      m_bottomBar(new QWidget(this)),
      m_meta(new MetaPanel(this)),
      m_pad(new KeyPad(m_bottomBar)),
      m_actions(new QWidget(m_bottomBar)),
      m_save(makeButton(QStringLiteral("Save"), QStringLiteral("EditorSave"), m_actions)),
      m_cancel(makeButton(QStringLiteral("Cancel"), QStringLiteral("EditorCancel"), m_actions)),
      m_export(makeButton(QStringLiteral("Export"), QStringLiteral("EditorExport"), this)),
      m_shadows(new ElevationLayer(this)),
      m_confirm(new ConfirmPopup(this)),
      m_toasts(new ToastHost(this))
{
    setObjectName(QStringLiteral("EditorWindow"));
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("Sheet Editor"));
    setWindowIcon(QIcon(QStringLiteral(":/icon/Icon9.ico")));
    setFixedSize(kWidth, kHeight);
    setFocusPolicy(Qt::StrongFocus);

    m_scroll->setObjectName(QStringLiteral("EditorGrid"));
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFocusPolicy(Qt::NoFocus);
    m_scroll->viewport()->setObjectName(QStringLiteral("EditorGridViewport"));
    m_grid->setObjectName(QStringLiteral("EditorGridCanvas"));
    m_grid->setBottomPadding(kGridBottom + kGridGap);
    m_scroll->setWidget(m_grid);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(kPadding, kPadding, kPadding, 0);
    root->addWidget(m_scroll);

    m_bottomBar->setObjectName(QStringLiteral("EditorBottomBar"));
    m_bottomBar->setAttribute(Qt::WA_StyledBackground);
    auto *actions = new QVBoxLayout(m_actions);
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(kActionGap);
    for (QPushButton *button : {m_save, m_cancel}) {
        button->setFixedWidth(kActionWidth);
        actions->addWidget(button);
    }
    m_actions->hide();

    m_shadows->track(m_meta, 12, [] { return std::vector<ShadowSpec>{currentTheme().editor.metaShadow}; });

    connect(m_grid, &NoteGrid::cellClicked, this, &EditorWindow::selectCell);
    connect(m_pad, &KeyPad::keyClicked, this, [this](int index) { toggleKey(index, false); });
    connect(m_save, &QPushButton::clicked, this, &EditorWindow::onSave);
    connect(m_cancel, &QPushButton::clicked, this, &EditorWindow::onCancel);
    connect(m_export, &QPushButton::clicked, this, &EditorWindow::exportSheet);
    for (int i = 0; i < 4; ++i) {
        EditableField *field = m_meta->field(static_cast<MetaPanel::Field>(i));
        connect(field, &EditableField::editRequested, this, [this, i] { beginEditing(i); });
        connect(field, &EditableField::commitRequested, this, &EditorWindow::commitEditing);
        connect(field, &EditableField::cancelRequested, this, &EditorWindow::cancelEditing);
    }
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        applyThemeTitleBar(this);
        update();
    });
    connect(&m_library, &LibraryService::changed, this, [this] {
        if (libraryIndex() < 0) {
            close();
        }
    });
    applyThemeTitleBar(this);
    positionBottomBar();
}

int EditorWindow::libraryIndex() const
{
    return m_payloadKey.empty() ? -1 : m_library.indexOfPayload(m_payloadKey);
}

void EditorWindow::openSheet(int index)
{
    const SheetEntry *row = m_library.entry(index);
    if (!row) {
        return;
    }
    if (row->payloadFile() == m_payloadKey) {
        return;
    }
    if (m_document.isDirty() || m_editingField >= 0) {
        QPointer<EditorWindow> self(this);
        const std::string key = row->payloadFile();
        m_confirm->ask(QStringLiteral("Discard changes to the music sheet?"), [self, key] {
            if (!self) {
                return;
            }
            const int current = self->m_library.indexOfPayload(key);
            if (current >= 0) {
                self->cancelEditing();
                self->load(current);
            }
        });
        return;
    }
    load(index);
}

void EditorWindow::load(int index)
{
    const SheetEntry *row = m_library.entry(index);
    if (!row) {
        return;
    }
    m_payloadKey = row->payloadFile();
    m_loaded = false;
    m_document = EditorDocument();
    m_payloadNotes.clear();
    m_hold = row->hasHoldNotes();
    clearSelection();

    const auto text = [](const std::string &value, const char *fallback) {
        return toQString(value.empty() ? std::string(fallback) : value);
    };
    m_meta->field(MetaPanel::Name)->setValue(text(row->name(), "Untitled"));
    m_meta->field(MetaPanel::Author)->setValue(text(row->author(), "Unknown"));
    m_meta->field(MetaPanel::TranscribedBy)->setValue(text(row->transcribedBy(), "Unknown"));
    const double bpm = row->bpm();
    m_meta->field(MetaPanel::Bpm)->setValue(std::isfinite(bpm) && bpm != 0 ? toQString(formatJsNumber(bpm))
                                                                           : QStringLiteral("120"));
    m_meta->setHoldSheet(m_hold);
    positionBottomBar();
    m_grid->setDocument(nullptr, 0);
    refreshActions();

    const quint64 generation = ++m_loadGeneration;
    QPointer<EditorWindow> self(this);
    m_library.loadPayloadAsync(index, [self, generation](Result<std::shared_ptr<const StoredSheet>> payload) {
        if (!self || generation != self->m_loadGeneration) {
            return;
        }
        if (!payload || !payload.value()) {
            Logger::warning("editor", payload ? "empty payload" : payload.error().details);
            return;
        }
        self->applyPayload(*payload.value());
    });
}

void EditorWindow::applyPayload(const StoredSheet &payload)
{
    m_document = EditorDocument(payload.keyMap);
    m_payloadNotes = payload.songNotes.value_or(std::vector<SongNote>{});
    m_hold = m_hold || payload.hasHoldNotes;
    m_meta->setHoldSheet(m_hold);
    m_loaded = true;
    m_scroll->verticalScrollBar()->setValue(0);
    m_grid->setDocument(&m_document, m_scroll->viewport()->height());
    refreshActions();
    positionBottomBar();
}

void EditorWindow::selectCell(int cell)
{
    if (!m_loaded) {
        return;
    }
    m_grid->setSelected(cell);
    m_pad->showKeys(m_document.keysAt(cell));
    refreshActions();
}

void EditorWindow::clearSelection()
{
    m_grid->setSelected(-1);
    m_pad->showKeys({});
    refreshActions();
}

void EditorWindow::toggleKey(int keyIndex, bool fromKeyboard)
{
    const int cell = m_grid->selected();
    if (m_hold || !m_loaded || cell < 0) {
        return;
    }
    const std::string &key = EditorDocument::keyAt(keyIndex);
    const bool active = m_document.toggle(cell, key);
    m_pad->setKeyActive(keyIndex, active);
    m_grid->animateDot(cell, keyIndex);
    if (fromKeyboard) {
        m_pad->flash(keyIndex);
    }
}

void EditorWindow::beginEditing(int field)
{
    if (m_hold) {
        return;
    }
    if (m_editingField >= 0 && m_editingField != field) {
        cancelEditing();
    }
    m_editingField = field;
    m_meta->field(static_cast<MetaPanel::Field>(field))->startEditing();
    refreshActions();
}

void EditorWindow::cancelEditing()
{
    if (m_editingField < 0) {
        return;
    }
    m_meta->field(static_cast<MetaPanel::Field>(m_editingField))->endEditing();
    m_editingField = -1;
    setFocus(Qt::OtherFocusReason);
    refreshActions();
}

void EditorWindow::commitEditing()
{
    if (m_editingField < 0) {
        return;
    }
    EditableField *field = m_meta->field(static_cast<MetaPanel::Field>(m_editingField));
    const QString text = field->editedText().trimmed();
    const int index = libraryIndex();
    OrderedJson value;
    if (m_editingField == MetaPanel::Bpm) {
        const auto bpm = parseLeadingInt(text);
        if (!bpm || *bpm <= 0) {
            notify(ToastType::Error, "Please enter a valid BPM number (must be greater than 0)", kErrorToastMs);
            cancelEditing();
            return;
        }
        value = *bpm;
    } else {
        value = fromQString(text);
    }
    if (auto saved = m_library.updateEntryField(index, kFieldKeys[static_cast<std::size_t>(m_editingField)], value);
        !saved) {
        notify(ToastType::Error, "Failed to save changes to the sheet", kErrorToastMs);
        cancelEditing();
        return;
    }
    field->setValue(value.is_string() ? toQString(value.get<std::string>()) : toQString(formatJsNumber(value.get<double>())));
    notify(ToastType::Success, "Changes saved successfully!", kSavedToastMs);
    cancelEditing();
}

void EditorWindow::saveGrid()
{
    if (m_hold || !m_loaded || !m_document.isDirty()) {
        return;
    }
    if (auto saved = m_library.saveEditedKeyMap(libraryIndex(), m_document.keyMap()); !saved) {
        notify(ToastType::Error, "Failed to save changes to the music sheet", kErrorToastMs);
        return;
    }
    m_document.markSaved();
    m_payloadNotes = songNotesFromKeyMap(m_document.keyMap());
    notify(ToastType::Success, "Sheet saved successfully!", kSavedToastMs);
    refreshActions();
}

void EditorWindow::onSave()
{
    if (m_hold) {
        return;
    }
    if (m_editingField >= 0) {
        commitEditing();
    } else {
        saveGrid();
    }
}

void EditorWindow::onCancel()
{
    if (m_hold) {
        return;
    }
    if (m_editingField >= 0) {
        cancelEditing();
        return;
    }
    if (!m_document.isDirty()) {
        return;
    }
    QPointer<EditorWindow> self(this);
    m_confirm->ask(QStringLiteral("Discard changes to the music sheet?"), [self] {
        if (!self) {
            return;
        }
        self->m_document.discard();
        self->m_grid->update();
        self->m_pad->showKeys(self->m_document.keysAt(self->m_grid->selected()));
    });
}

void EditorWindow::exportSheet()
{
    const SheetEntry *row = m_library.entry(libraryIndex());
    if (!row || !m_loaded) {
        notify(ToastType::Warning, "No sheet loaded to export.", kErrorToastMs);
        return;
    }
    const std::vector<SongNote> notes = m_document.isDirty() ? songNotesFromKeyMap(m_document.keyMap()) : m_payloadNotes;
    const OrderedJson document = buildExportDocument(*row, notes, m_document.keyMap());
    const QString name = toQString(row->name().empty() ? std::string("Sheet") : row->name());
    const auto path = chooseSheetExportPath(this, name + QStringLiteral(".txt"));
    if (!path) {
        return;
    }
    const std::string content = serializeExport(document, exportLayoutForPath(path->string()));
    const std::filesystem::path file = *path;
    QThreadPool::globalInstance()->start([file, content] {
        if (auto written = LibraryService::writeExportFile(file, content); !written) {
            Logger::error("editor", written.error().details);
        }
    });
}

void EditorWindow::refreshActions()
{
    const bool visible = !m_hold && (m_grid->selected() >= 0 || m_editingField >= 0);
    m_actions->setVisible(visible);
    for (QPushButton *button : {m_save, m_cancel}) {
        button->setEnabled(!m_hold);
        button->setToolTip(m_hold ? kHoldTooltip : QString());
    }
}

void EditorWindow::notify(ToastType type, const std::string &message, int durationMs)
{
    m_toasts->show(Toast{type, {}, message, durationMs});
}

void EditorWindow::keyPressEvent(QKeyEvent *event)
{
    if (m_editingField >= 0) {
        QWidget::keyPressEvent(event);
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        clearSelection();
        return;
    }
    if (event->matches(QKeySequence::Save) || (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_S)) {
        saveGrid();
        return;
    }
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        QWidget::keyPressEvent(event);
        return;
    }
    const int index = EditorDocument::keyIndex(fromQString(event->text().toLower()));
    if (index >= 0 && !event->isAutoRepeat()) {
        toggleKey(index, true);
        return;
    }
    QWidget::keyPressEvent(event);
}


void EditorWindow::positionBottomBar()
{
    const QSize pad = m_pad->sizeHint();
    const int barHeight = kBarTop + pad.height() + kBarBottom;
    m_bottomBar->setGeometry(0, height() - barHeight, width(), barHeight);
    const int padLeft = (width() - pad.width()) / 2;
    m_pad->setGeometry(padLeft, kBarTop, pad.width(), pad.height());
    m_meta->resize(MetaPanel::kMaxWidth, m_meta->layout()->totalHeightForWidth(MetaPanel::kMaxWidth));
    m_meta->move(kMetaLeft, height() - kMetaBottom - m_meta->height());
    m_export->adjustSize();
    m_export->move(static_cast<int>(std::round(width() - kExportRight - m_export->width())),
                   height() - kExportBottom - m_export->height());
    const QSize actions = m_actions->sizeHint();
    const int exportTopInBar = m_export->y() - m_bottomBar->y();
    const int centeredTop = kBarTop + (pad.height() - actions.height()) / 2;
    const int top = std::min(centeredTop, exportTopInBar - kActionGap - actions.height());
    m_actions->setGeometry(padLeft + pad.width() + kActionsOffset, top, actions.width(), actions.height());
    m_bottomBar->raise();
    m_shadows->setFloating(true);
    m_meta->raise();
    m_export->raise();
    m_toasts->relayout();
}

void EditorWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    positionBottomBar();
}

}  // namespace sky
