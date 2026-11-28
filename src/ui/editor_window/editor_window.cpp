#include "ui/editor_window/editor_window.hpp"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPointer>
#include <QGuiApplication>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

#include "core/translator.hpp"
#include "codec/sheet_exporter.hpp"
#include "core/logger.hpp"
#include "core/string_utils.hpp"
#include "model/key_table.hpp"
#include "platform/file_dialog.hpp"
#include "platform/window_attributes.hpp"
#include "services/audio/sample_bank.hpp"
#include "services/audio/sheet_audio.hpp"
#include "services/library/library_service.hpp"
#include "services/settings/ui_state_service.hpp"
#include "ui/core/elevation_layer.hpp"
#include "ui/editor_window/editable_field.hpp"
#include "ui/editor_window/editor_setup.hpp"
#include "ui/editor_window/key_colors.hpp"
#include "ui/editor_window/key_pad.hpp"
#include "ui/editor_window/key_pad_cell.hpp"
#include "ui/editor_window/meta_panel.hpp"
#include "ui/editor_window/step_grid_view.hpp"
#include "ui/editor_window/timeline_view.hpp"
#include "ui/editor_window/tool_button.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/confirm_popup.hpp"
#include "ui/widgets/flow_layout.hpp"
#include "ui/widgets/select_box.hpp"
#include "ui/widgets/toast_host.hpp"

namespace sky {

namespace {

constexpr int kWidth = 1480;
constexpr int kHeight = 900;
constexpr int kMinWidth = 760;
constexpr int kMinHeight = 540;
constexpr int kWideWidth = 1240;
constexpr int kMediumWidth = 980;
constexpr int kMediumSideWidth = 304;
constexpr int kStackedSongWidth = 290;
constexpr int kDetailsMinWindowHeight = 820;
constexpr int kStackedMaxCell = 46;
constexpr int kMinPadCell = 30;
constexpr int kComfortPadCell = 34;
constexpr int kTimelineComfort = 290;
constexpr int kStatusHeight = 20;
constexpr int kToggleHeight = 28;
constexpr int kScreenShare = 90;
constexpr int kPadding = 16;
constexpr int kSectionGap = 12;
constexpr int kToolGap = 6;
constexpr int kButtonIcon = 15;
constexpr int kPlayFrameMs = 16;
constexpr const char *kSoundpackInstrument = "Piano";
constexpr int kAudioIdleMs = 3000;
constexpr int kGroupGap = 16;
constexpr int kActionGap = 8;
constexpr int kSideWidth = 372;
constexpr int kSideGap = 10;
constexpr int kSavedToastMs = 2000;
constexpr int kErrorToastMs = 3000;
constexpr int kInfoToastMs = 1500;
constexpr int kBeatsPerBar = 4;
constexpr std::array<int, 6> kSubdivisions = {1, 2, 3, 4, 6, 8};

constexpr std::array<const char *, 4> kFieldKeys = {"name", "author", "transcribedBy", "bpm"};

constexpr const char *kDiscardPrompt = trNoop("Discard changes to the music sheet?");

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

QString clockText(double timeMs)
{
    const auto ms = static_cast<qint64>(std::llround(std::max(0.0, timeMs)));
    return QStringLiteral("%1:%2.%3")
        .arg(ms / 60000)
        .arg((ms % 60000) / 1000, 2, 10, QLatin1Char('0'))
        .arg(ms % 1000, 3, 10, QLatin1Char('0'));
}

QString countText(int count, const char *one, const char *many)
{
    return count == 1 ? tr(one) : tr(many).arg(count);
}

QPushButton *makeButton(const QString &text, const QString &objectName, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::TabFocus);
    return button;
}

QLabel *makeLabel(const QString &objectName, QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setObjectName(objectName);
    label->setTextFormat(Qt::PlainText);
    return label;
}

int subdivisionIndex(int subdivision)
{
    const auto found = std::find(kSubdivisions.begin(), kSubdivisions.end(), subdivision);
    return found == kSubdivisions.end() ? 0 : static_cast<int>(found - kSubdivisions.begin());
}

}  // namespace

EditorWindow::EditorWindow(LibraryService &library, UiStateService &uiState, std::filesystem::path soundFolder,
                           QWidget *owner)
    : QWidget(owner, Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint |
                         Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint),
      m_library(library),
      m_uiState(uiState),
      m_undo(new ToolButton(QStringLiteral("arrow-counterclockwise"), tr("Undo (Ctrl+Z)"), this)),
      m_redo(new ToolButton(QStringLiteral("arrow-clockwise"), tr("Redo (Ctrl+Y)"), this)),
      m_insert(new ToolButton(QStringLiteral("plus-square"),
                              tr("Add column: insert an empty column after the selection (E)"), this)),
      m_delete(new ToolButton(QStringLiteral("dash-square"),
                              tr("Remove column: delete the selected columns and pull the rest forward (Q)"),
                              this)),
      m_erase(new ToolButton(QStringLiteral("eraser"),
                             tr("Clear notes: remove the notes but keep the time (Backspace)"), this)),
      m_copy(new ToolButton(QStringLiteral("copy"), tr("Copy columns (Ctrl+C)"), this)),
      m_cut(new ToolButton(QStringLiteral("scissors"), tr("Cut columns (Ctrl+X)"), this)),
      m_paste(new ToolButton(QStringLiteral("clipboard"),
                             tr("Paste at the cursor (Ctrl+V). Ctrl+Shift+V pastes on top of what is there."),
                             this)),
      m_keyUp(new ToolButton(QStringLiteral("arrow-up"), tr("Key up: move the notes one key higher (Alt+Up)"),
                             this)),
      m_keyDown(new ToolButton(QStringLiteral("arrow-down"),
                               tr("Key down: move the notes one key lower (Alt+Down)"), this)),
      m_snap(new ToolButton(QStringLiteral("magnet"),
                            tr("Snap to grid: move notes onto the columns. Works on the selection, or the "
                                           "whole sheet when one column is selected."),
                            this)),
      m_earlier(new ToolButton(QStringLiteral("chevron-bar-left"),
                               tr("Earlier: play the notes one column sooner (Alt+Left)"), this)),
      m_later(new ToolButton(QStringLiteral("chevron-bar-right"),
                             tr("Later: play the notes one column later (Alt+Right)"), this)),
      m_holdLess(new ToolButton(QStringLiteral("arrows-collapse-vertical"),
                                tr("Shorter hold: hold the notes one column less ([)"), this)),
      m_holdMore(new ToolButton(QStringLiteral("arrows-expand-vertical"),
                                tr("Longer hold: hold the notes one column more (])"), this)),
      m_simpleView(new ToolButton(QStringLiteral("grid-3x3-gap"),
                                  tr("Simple view: tiles laid out like Sky's keyboard"), this)),
      m_advancedView(new ToolButton(QStringLiteral("bar-chart-steps"),
                                    tr("Advanced view: a timeline with every key on its own row"), this)),
      m_zoomOut(new ToolButton(QStringLiteral("zoom-out"), tr("Zoom out (Ctrl+-, Ctrl+wheel)"), this)),
      m_zoomIn(new ToolButton(QStringLiteral("zoom-in"), tr("Zoom in (Ctrl+=, Ctrl+wheel)"), this)),
      m_gridSelect(new SelectBox(this)),
      m_timeline(new TimelineView(this)),
      m_title(makeLabel(QStringLiteral("EditorTitle"), this)),
      m_subtitle(makeLabel(QStringLiteral("EditorSubtitle"), this)),
      m_saveState(makeLabel(QStringLiteral("EditorSaveState"), this)),
      m_selectionTitle(makeLabel(QStringLiteral("EditorSelectionTitle"), this)),
      m_selectionDetail(makeLabel(QStringLiteral("EditorSelectionDetail"), this)),
      m_hint(makeLabel(QStringLiteral("EditorHint"), this)),
      m_status(makeLabel(QStringLiteral("EditorStatus"), this)),
      m_meta(new MetaPanel(this)),
      m_pad(new KeyPad(this)),
      m_save(makeButton(tr("Save"), QStringLiteral("EditorSave"), this)),
      m_cancel(makeButton(tr("Discard"), QStringLiteral("EditorCancel"), this)),
      m_export(makeButton(tr("Export"), QStringLiteral("EditorExport"), this)),
      m_shadows(new ElevationLayer(this)),
      m_confirm(new ConfirmPopup(this)),
      m_toasts(new ToastHost(this))
{
    setObjectName(QStringLiteral("EditorWindow"));
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Sheet Editor[*]"));
    setWindowIcon(QIcon(QStringLiteral(":/icon/Icon9.ico")));
    setFocusPolicy(Qt::StrongFocus);

    m_play = new ToolButton(QStringLiteral("play-fill"), tr("Play from the selected column (Space)"), this);
    m_sound = new ToolButton(QStringLiteral("volume-up"), QString(), this);
    m_audio = std::make_unique<SheetAudio>();
    m_audio->setMuted(m_uiState.editorSoundMuted());
    m_soundFolder = std::move(soundFolder);
    loadSounds();
    m_playTimer = new QTimer(this);
    m_playTimer->setInterval(kPlayFrameMs);
    m_idleTimer = new QTimer(this);
    m_idleTimer->setInterval(kAudioIdleMs);
    refreshSoundButton();
    m_simpleView->setLabel(tr("Simple"));
    m_advancedView->setLabel(tr("Advanced"));
    m_simpleView->setCheckable(true);
    m_advancedView->setCheckable(true);
    m_steps = new StepGridView();
    m_stepScroll = new QScrollArea(this);
    m_stepScroll->setObjectName(QStringLiteral("EditorSteps"));
    m_stepScroll->setFrameShape(QFrame::NoFrame);
    m_stepScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_stepScroll->setWidgetResizable(true);
    m_stepScroll->setFocusPolicy(Qt::NoFocus);
    m_stepScroll->viewport()->setObjectName(QStringLiteral("EditorStepsViewport"));
    m_steps->setObjectName(QStringLiteral("EditorStepsCanvas"));
    m_stepScroll->setWidget(m_steps);
    m_views = new QStackedWidget(this);
    m_views->addWidget(m_stepScroll);
    m_views->addWidget(m_timeline);

    for (int subdivision : kSubdivisions) {
        m_gridSelect->addOption(subdivision == 1 ? tr("1 beat")
                                                 : tr("1/%1 beat").arg(subdivision),
                                subdivision == 1 ? tr("One column is all of a beat")
                                                 : tr("One column is 1/%1 of a beat").arg(subdivision));
    }
    m_gridSelect->setCurrentIndex(0);
    m_gridSelect->setFocusPolicy(Qt::NoFocus);
    m_gridSelect->setToolTip(tr("How much time one column holds"));
    m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_status->setToolTip(tr(
        "Click a cell to add or remove a note. Drag a note to move it (hold Alt to leave the grid), drag its tail or "
        "Shift+drag it to hold it longer. Drag across empty cells or the ruler to select columns."));
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_title->installEventFilter(this);
    m_subtitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_selectionDetail->setWordWrap(true);
    m_hint->setWordWrap(true);
    m_hint->setVisible(false);
    m_save->setToolTip(tr("Save the notes (Ctrl+S)"));
    m_cancel->setToolTip(tr("Throw away the changes since the last save"));
    m_export->setToolTip(tr("Save a copy as a .txt, .json or .skysheet file"));

    auto *titles = new QVBoxLayout();
    titles->setSpacing(2);
    titles->addWidget(m_title);
    titles->addWidget(m_subtitle);
    auto *header = new QHBoxLayout();
    header->setSpacing(kActionGap);
    header->addLayout(titles, 1);
    auto *tabsBox = new QWidget(this);
    tabsBox->setObjectName(QStringLiteral("EditorViewTabs"));
    tabsBox->setAttribute(Qt::WA_StyledBackground);
    auto *tabs = new QHBoxLayout(tabsBox);
    tabs->setContentsMargins(3, 3, 3, 3);
    tabs->setSpacing(2);
    tabs->addWidget(m_simpleView);
    tabs->addWidget(m_advancedView);
    header->addWidget(tabsBox);
    header->addSpacing(kGroupGap);
    header->addWidget(m_saveState);
    header->addSpacing(kGroupGap);
    header->addWidget(m_cancel);
    header->addWidget(m_export);
    header->addWidget(m_save);

    auto *toolbar = new QWidget(this);
    m_toolbar = toolbar;
    toolbar->setObjectName(QStringLiteral("EditorToolbar"));
    auto *tools = new QHBoxLayout(toolbar);
    tools->setContentsMargins(0, 0, 0, 0);
    tools->setSpacing(kGroupGap);
    auto *editTools = new QWidget(toolbar);
    QSizePolicy flowPolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    flowPolicy.setHeightForWidth(true);
    editTools->setSizePolicy(flowPolicy);
    auto *flow = new FlowLayout(editTools, kToolGap / 3, kToolGap);
    flow->setContentsMargins(kToolGap, kToolGap / 2, kToolGap, kToolGap / 2);
    const auto group = [flow](std::initializer_list<QWidget *> widgets) {
        int gap = flow->count() > 0 ? kGroupGap : 0;
        for (QWidget *widget : widgets) {
            flow->addWidget(widget, gap);
            gap = 0;
        }
    };
    group({m_play, m_sound});
    group({m_undo, m_redo});
    group({m_insert, m_delete, m_erase});
    group({m_copy, m_cut, m_paste});
    group({m_earlier, m_later, m_holdLess, m_holdMore});
    group({m_keyUp, m_keyDown, m_snap});
    tools->addWidget(editTools, 1);
    QLabel *gridLabel = makeLabel(QStringLiteral("EditorCaption"), toolbar);
    gridLabel->setText(tr("GRID"));
    group({gridLabel, m_gridSelect, m_zoomOut, m_zoomIn});

    auto *main = new QVBoxLayout();
    main->setSpacing(kActionGap);
    main->addWidget(m_views, 1);
    main->addWidget(m_status);

    m_side = new QWidget(this);
    m_side->setObjectName(QStringLiteral("EditorSide"));
    m_side->setAttribute(Qt::WA_StyledBackground);
    auto *info = new QWidget(m_side);
    m_info = info;
    auto *infoLayout = new QVBoxLayout(info);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(kSideGap);
    QLabel *selectionCaption = makeLabel(QStringLiteral("EditorCaption"), info);
    selectionCaption->setText(tr("SELECTED"));
    infoLayout->addWidget(selectionCaption);
    infoLayout->addWidget(m_selectionTitle);
    infoLayout->addWidget(m_selectionDetail);
    infoLayout->addWidget(m_hint);
    infoLayout->addStretch(1);
    m_songBox = new QWidget(m_side);
    auto *songLayout = new QVBoxLayout(m_songBox);
    songLayout->setContentsMargins(0, 0, 0, 0);
    songLayout->setSpacing(kSideGap);
    m_detailsToggle = makeButton(QString(), QStringLiteral("EditorSectionToggle"), m_songBox);
    m_detailsToggle->setFocusPolicy(Qt::NoFocus);
    songLayout->addWidget(m_detailsToggle, 0, Qt::AlignLeft);
    songLayout->addWidget(m_meta);
    connect(m_detailsToggle, &QAbstractButton::clicked, this, [this] {
        m_detailsChosen = true;
        setDetailsOpen(!m_detailsOpen);
        applyResponsiveLayout();
    });
    songLayout->addStretch(1);
    m_sideLayout = new QBoxLayout(QBoxLayout::TopToBottom, m_side);
    m_sideLayout->setContentsMargins(kSideGap * 2, kSideGap * 2, kSideGap * 2, kSideGap * 2);
    m_sideLayout->setSpacing(kSideGap);
    m_sideLayout->addWidget(info);
    m_sideLayout->addWidget(m_pad, 0, Qt::AlignHCenter);
    m_sideLayout->addStretch(1);
    m_sideLayout->addWidget(m_songBox);

    m_body = new QBoxLayout(QBoxLayout::LeftToRight);
    m_body->setSpacing(kSectionGap + 4);
    m_body->addLayout(main, 1);
    m_body->addWidget(m_side);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    root->setSpacing(kSectionGap);
    root->addLayout(header);
    root->addWidget(toolbar);
    root->addLayout(m_body, 1);

    m_shadows->track(m_meta, 12, [] { return std::vector<ShadowSpec>{currentTheme().editor.metaShadow}; });
    m_timeline->setDocument(&m_document);
    m_steps->setDocument(&m_document);
    setColorfulNotes(m_uiState.editorColorfulNotes());
    setAdvancedView(m_uiState.editorAdvancedView());
    m_setup = new EditorSetup(this);
    connect(m_setup, &EditorSetup::chosen, this, &EditorWindow::applySetup);

    connect(m_steps, &StepGridView::columnPressed, this, &EditorWindow::pressColumn);
    connect(m_play, &QAbstractButton::clicked, this, &EditorWindow::togglePlayback);
    connect(m_sound, &QAbstractButton::clicked, this, &EditorWindow::toggleSound);
    connect(m_playTimer, &QTimer::timeout, this, &EditorWindow::playbackTick);
    connect(m_idleTimer, &QTimer::timeout, this, [this] {
        if (!m_playing && m_audio->idle()) {
            m_audio->release();
            m_idleTimer->stop();
        }
    });
    connect(m_steps, &StepGridView::cellToggled, this, &EditorWindow::toggleCell);
    connect(m_steps, &StepGridView::columnsDragged, this, &EditorWindow::dragColumn);
    connect(m_simpleView, &QAbstractButton::clicked, this, [this] { setAdvancedView(false); });
    connect(m_advancedView, &QAbstractButton::clicked, this, [this] { setAdvancedView(true); });
    connect(m_earlier, &QAbstractButton::clicked, this, [this] { moveSelection(-1); });
    connect(m_later, &QAbstractButton::clicked, this, [this] { moveSelection(1); });
    connect(m_holdLess, &QAbstractButton::clicked, this, [this] { adjustHold(-1); });
    connect(m_holdMore, &QAbstractButton::clicked, this, [this] { adjustHold(1); });
    connect(m_timeline, &TimelineView::columnPressed, this, &EditorWindow::pressColumn);
    connect(m_timeline, &TimelineView::cellToggled, this, &EditorWindow::toggleCell);
    connect(m_timeline, &TimelineView::noteMoved, this, &EditorWindow::moveNote);
    connect(m_timeline, &TimelineView::holdChanged, this, &EditorWindow::changeHold);
    connect(m_pad, &KeyPad::keyClicked, this, [this](int index) { toggleKey(index, false); });
    connect(m_undo, &QAbstractButton::clicked, this, &EditorWindow::undoEdit);
    connect(m_redo, &QAbstractButton::clicked, this, &EditorWindow::redoEdit);
    connect(m_insert, &QAbstractButton::clicked, this, &EditorWindow::insertColumn);
    connect(m_delete, &QAbstractButton::clicked, this, &EditorWindow::deleteColumns);
    connect(m_erase, &QAbstractButton::clicked, this, &EditorWindow::eraseColumns);
    connect(m_copy, &QAbstractButton::clicked, this, [this] { copyColumns(false); });
    connect(m_cut, &QAbstractButton::clicked, this, [this] { copyColumns(true); });
    connect(m_paste, &QAbstractButton::clicked, this, [this] { pasteColumns(true); });
    connect(m_keyUp, &QAbstractButton::clicked, this, [this] { shiftKeys(1); });
    connect(m_keyDown, &QAbstractButton::clicked, this, [this] { shiftKeys(-1); });
    connect(m_snap, &QAbstractButton::clicked, this, &EditorWindow::snapToGrid);
    connect(m_zoomOut, &QAbstractButton::clicked, this, [this] { zoomViews(-1); });
    connect(m_zoomIn, &QAbstractButton::clicked, this, [this] { zoomViews(1); });
    connect(m_gridSelect, &SelectBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0 && index < static_cast<int>(kSubdivisions.size())) {
            const int subdivision = kSubdivisions[static_cast<std::size_t>(index)];
            setGrid(TimeGrid(m_grid.bpm(), subdivision, m_grid.originMs()));
        }
    });
    connect(m_save, &QAbstractButton::clicked, this, &EditorWindow::onSave);
    connect(m_cancel, &QAbstractButton::clicked, this, &EditorWindow::onCancel);
    connect(m_export, &QAbstractButton::clicked, this, &EditorWindow::exportSheet);
    for (int i = 0; i < 4; ++i) {
        EditableField *field = m_meta->field(static_cast<MetaPanel::Field>(i));
        connect(field, &EditableField::editRequested, this, [this, i] { beginEditing(i); });
        connect(field, &EditableField::commitRequested, this, &EditorWindow::commitEditing);
        connect(field, &EditableField::cancelRequested, this, &EditorWindow::cancelEditing);
    }
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        applyThemeTitleBar(this);
        refreshButtonIcons();
        update();
    });
    refreshButtonIcons();
    connect(&m_library, &LibraryService::changed, this, [this] {
        if (libraryIndex() < 0) {
            m_closeConfirmed = true;
            close();
        }
    });
    applyThemeTitleBar(this);
    fitToScreen(owner);
    applyResponsiveLayout();
    m_confirm->raise();
    refreshActions();
}

void EditorWindow::fitToScreen(QWidget *owner)
{
    QScreen *screen = owner && owner->screen() ? owner->screen() : QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, kWidth, kHeight);
    setMinimumSize(std::min(kMinWidth, available.width()), std::min(kMinHeight, available.height()));
    const int width = std::clamp(available.width() * kScreenShare / 100, minimumWidth(), kWidth);
    const int height = std::clamp(available.height() * kScreenShare / 100, minimumHeight(), kHeight);
    resize(width, height);
    move(available.x() + (available.width() - width) / 2, available.y() + (available.height() - height) / 2);
}

int EditorWindow::chromeHeight() const
{
    const int header = std::max(m_title->sizeHint().height() + m_subtitle->sizeHint().height() + 2,
                                m_save->sizeHint().height());
    const int toolbar = m_toolbar->hasHeightForWidth() ? m_toolbar->heightForWidth(width() - kPadding * 2)
                                                       : m_toolbar->sizeHint().height();
    return kPadding * 2 + kSectionGap * 2 + header + std::max(toolbar, m_toolbar->sizeHint().height());
}

void EditorWindow::applyResponsiveLayout()
{
    if (!m_body) {
        return;
    }
    const int w = width();
    const int h = height();
    m_layout = w >= kWideWidth ? Layout::Wide : (w >= kMediumWidth ? Layout::Medium : Layout::Stacked);
    if (!m_detailsChosen) {
        setDetailsOpen(m_layout != Layout::Stacked && h >= kDetailsMinWindowHeight);
    }
    const int padChrome = kSideGap * 4;
    const auto cellFor = [](int padHeight) { return (padHeight - 8) * 2 / 7; };
    const int details = m_detailsOpen ? m_meta->sizeHint().height() + kSideGap : 0;

    if (m_layout == Layout::Stacked) {
        m_body->setDirection(QBoxLayout::TopToBottom);
        m_sideLayout->setDirection(QBoxLayout::LeftToRight);
        m_sideLayout->setAlignment(m_pad, Qt::AlignVCenter);
        m_side->setMinimumWidth(0);
        m_side->setMaximumWidth(QWIDGETSIZE_MAX);
        m_songBox->setFixedWidth(kStackedSongWidth);
        m_sideLayout->setStretch(0, 1);
        m_sideLayout->setStretch(2, 0);
        m_hintRoom = false;
        const int room = h - chromeHeight() - kStatusHeight - kActionGap - kSectionGap - kTimelineComfort - padChrome;
        const int cell = std::min({kStackedMaxCell, cellFor(room), cellFor(room - details)});
        m_side->setVisible(cell >= kMinPadCell);
        m_pad->setCellSize(cell);
        refreshStatus();
        return;
    }

    const int sideWidth = m_layout == Layout::Wide ? kSideWidth : kMediumSideWidth;
    m_side->setVisible(true);
    m_body->setDirection(QBoxLayout::LeftToRight);
    m_sideLayout->setDirection(QBoxLayout::TopToBottom);
    m_sideLayout->setAlignment(m_pad, Qt::AlignHCenter);
    m_side->setFixedWidth(sideWidth);
    m_songBox->setMinimumWidth(0);
    m_songBox->setMaximumWidth(QWIDGETSIZE_MAX);
    m_sideLayout->setStretch(0, 0);
    m_sideLayout->setStretch(2, 1);
    int widthCell = KeyPadCell::kSize;
    while (widthCell > kMinPadCell && KeyPad::widthFor(widthCell) > sideWidth - padChrome) {
        --widthCell;
    }
    const int inner = sideWidth - padChrome;
    const int room = h - chromeHeight() - padChrome - kSideGap * 3 - kToggleHeight - details;
    m_hintRoom = true;
    refreshStatus();
    int cell = cellFor(room - m_info->layout()->totalHeightForWidth(inner));
    if (cell < kComfortPadCell && m_hint->isVisibleTo(m_info)) {
        m_hintRoom = false;
        refreshStatus();
        cell = cellFor(room - m_info->layout()->totalHeightForWidth(inner));
    }
    m_pad->setCellSize(std::min(widthCell, cell));
}

void EditorWindow::setDetailsOpen(bool open)
{
    m_detailsOpen = open;
    m_meta->setVisible(open);
    m_detailsToggle->setText(open ? tr("Hide song details") : tr("Show song details"));
}

void EditorWindow::elideTitle()
{
    m_title->setText(m_title->fontMetrics().elidedText(m_fullTitle, Qt::ElideRight, std::max(0, m_title->width())));
}

bool EditorWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_title && (event->type() == QEvent::Resize || event->type() == QEvent::FontChange ||
                               event->type() == QEvent::StyleChange)) {
        elideTitle();
    }
    return QWidget::eventFilter(watched, event);
}

void EditorWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    applyResponsiveLayout();
}

EditorWindow::~EditorWindow()
{
    m_playTimer->stop();
    m_audio->release();
}

int EditorWindow::libraryIndex() const
{
    return m_payloadKey.empty() ? -1 : m_library.indexOfPayload(m_payloadKey);
}

double EditorWindow::sheetBpm() const
{
    const SheetEntry *row = m_library.entry(libraryIndex());
    return row ? row->bpm() : 0.0;
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
        m_confirm->ask(tr(kDiscardPrompt), [self, key] {
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
    if (m_playing) {
        stopPlayback();
    }
    m_payloadKey = row->payloadFile();
    m_loaded = false;
    m_document = EditorDocument();
    m_clip = ColumnClip{};
    m_cursor = -1;
    m_anchor = -1;
    m_timeline->setDocument(&m_document);
    m_steps->setDocument(&m_document);
    m_pad->showKeys({});

    const auto text = [](const std::string &value, const char *fallback) {
        return value.empty() ? tr(fallback) : toQString(value);
    };
    m_meta->field(MetaPanel::Name)->setValue(text(row->name(), trNoop("Untitled")));
    m_meta->field(MetaPanel::Author)->setValue(text(row->author(), trNoop("Unknown")));
    m_meta->field(MetaPanel::TranscribedBy)->setValue(text(row->transcribedBy(), trNoop("Unknown")));
    const double bpm = row->bpm();
    m_meta->field(MetaPanel::Bpm)->setValue(std::isfinite(bpm) && bpm != 0 ? toQString(formatJsNumber(bpm))
                                                                           : QStringLiteral("120"));
    refreshHeader();
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
    std::vector<EditorNote> notes = EditorDocument::notesFrom(payload);
    const GridGuess guess = EditorDocument::detectGrid(notes, sheetBpm());
    m_document = EditorDocument(std::move(notes));
    m_loaded = true;
    m_steady = guess.steady;
    m_grid = guess.grid;
    m_gridSelect->blockSignals(true);
    m_gridSelect->setCurrentIndex(subdivisionIndex(m_grid.subdivision()));
    m_gridSelect->blockSignals(false);
    m_timeline->setDocument(&m_document);
    m_timeline->setGrid(m_grid);
    m_steps->setDocument(&m_document);
    m_steps->setGrid(m_grid);
    moveCursorTo(m_document.empty() ? 0 : m_grid.columnOf(static_cast<double>(m_document.notes().front().time)), false);
}

void EditorWindow::setGrid(const TimeGrid &grid)
{
    const std::int64_t cursorTime = m_cursor >= 0 ? m_grid.timeOf(m_cursor) : 0;
    const std::int64_t anchorTime = m_anchor >= 0 ? m_grid.timeOf(m_anchor) : cursorTime;
    m_grid = grid;
    m_timeline->setGrid(m_grid);
    m_steps->setGrid(m_grid);
    m_anchor = m_grid.columnOf(static_cast<double>(anchorTime));
    moveCursorTo(m_grid.columnOf(static_cast<double>(cursorTime)), true);
}

void EditorWindow::moveCursorTo(int column, bool extend)
{
    if (!m_loaded) {
        return;
    }
    column = std::max(0, column);
    if (!extend || m_anchor < 0 || m_cursor < 0) {
        m_anchor = column;
    }
    m_cursor = column;
    const auto [first, last] = selection();
    m_timeline->setSelection(m_cursor, first, last);
    m_steps->setSelection(m_cursor, first, last);
    if (m_advanced) {
        m_timeline->ensureColumnVisible(m_cursor);
    } else {
        const QRect bounds = m_steps->tileBounds(m_cursor);
        m_stepScroll->ensureVisible(bounds.center().x(), bounds.center().y(), 0, bounds.height() / 2 + 6);
    }
    m_pad->showKeys(m_document.keysInColumn(m_cursor, m_grid));
    refreshActions();
}

std::pair<int, int> EditorWindow::selection() const
{
    if (m_cursor < 0) {
        return {-1, -1};
    }
    const int anchor = m_anchor < 0 ? m_cursor : m_anchor;
    return {std::min(anchor, m_cursor), std::max(anchor, m_cursor)};
}

void EditorWindow::afterEdit()
{
    refreshViews();
    moveCursorTo(std::max(0, m_cursor), true);
}

void EditorWindow::toggleKey(int key, bool fromKeyboard)
{
    if (!m_loaded || m_cursor < 0) {
        return;
    }
    const bool active = m_document.toggleInColumn(m_cursor, key, m_grid);
    m_pad->setKeyActive(key, active);
    if (active) {
        m_audio->preview(key);
        m_idleTimer->start();
    }
    if (fromKeyboard) {
        m_pad->flash(key);
    }
    afterEdit();
}

void EditorWindow::toggleCell(int column, int key)
{
    moveCursorTo(column, false);
    toggleKey(key, false);
}

void EditorWindow::moveNote(qint64 time, int key, qint64 newTime, int newKey)
{
    if (!m_loaded) {
        return;
    }
    if (!m_document.moveNote(time, key, newTime, newKey)) {
        notify(ToastType::Warning, trUtf8("There is already a note there."), kErrorToastMs);
        return;
    }
    refreshViews();
    moveCursorTo(m_grid.columnOf(static_cast<double>(newTime)), false);
}

void EditorWindow::changeHold(qint64 time, int key, qint64 hold)
{
    if (m_loaded && m_document.setHold(time, key, hold)) {
        afterEdit();
    }
}

void EditorWindow::undoEdit()
{
    if (m_loaded && m_document.undo()) {
        afterEdit();
    }
}

void EditorWindow::redoEdit()
{
    if (m_loaded && m_document.redo()) {
        afterEdit();
    }
}

void EditorWindow::insertColumn()
{
    if (!m_loaded || m_cursor < 0) {
        return;
    }
    const int at = selection().second + 1;
    m_document.insertColumns(at, 1, m_grid);
    refreshViews();
    moveCursorTo(at, false);
}

void EditorWindow::deleteColumns()
{
    const auto [first, last] = selection();
    if (!m_loaded || first < 0) {
        return;
    }
    m_document.deleteColumns(first, last, m_grid);
    refreshViews();
    moveCursorTo(first, false);
}

void EditorWindow::eraseColumns()
{
    const auto [first, last] = selection();
    if (m_loaded && m_document.eraseColumns(first, last, m_grid)) {
        afterEdit();
    }
}

void EditorWindow::copyColumns(bool cut)
{
    const auto [first, last] = selection();
    if (!m_loaded || first < 0) {
        return;
    }
    m_clip = m_document.copyColumns(first, last, m_grid);
    const int columns = m_clip.columns;
    if (cut) {
        m_document.deleteColumns(first, last, m_grid);
        refreshViews();
        moveCursorTo(first, false);
        notify(ToastType::Info, fromQString(columns == 1 ? tr("1 column cut") : tr("%1 columns cut").arg(columns)),
               kInfoToastMs);
        return;
    }
    refreshActions();
    notify(ToastType::Info, fromQString(columns == 1 ? tr("1 column copied") : tr("%1 columns copied").arg(columns)),
           kInfoToastMs);
}

void EditorWindow::pasteColumns(bool insert)
{
    if (!m_loaded || m_clip.empty() || m_cursor < 0) {
        return;
    }
    const int at = m_cursor;
    m_document.pasteColumns(at, m_clip, m_grid, insert);
    refreshViews();
    m_anchor = at;
    m_cursor = at + m_clip.columns - 1;
    moveCursorTo(m_cursor, true);
}

void EditorWindow::duplicateColumns()
{
    const auto [first, last] = selection();
    if (!m_loaded || first < 0) {
        return;
    }
    const ColumnClip copy = m_document.copyColumns(first, last, m_grid);
    m_document.pasteColumns(last + 1, copy, m_grid, true);
    refreshViews();
    m_anchor = last + 1;
    m_cursor = last + copy.columns;
    moveCursorTo(m_cursor, true);
}

void EditorWindow::shiftKeys(int delta)
{
    const auto [first, last] = selection();
    if (!m_loaded || first < 0) {
        return;
    }
    if (m_document.shiftKeys(first, last, delta, m_grid)) {
        afterEdit();
        return;
    }
    if (!m_document.copyColumns(first, last, m_grid).notes.empty()) {
        notify(ToastType::Warning,
               delta > 0 ? trUtf8("A note is already on the highest key.")
                         : trUtf8("A note is already on the lowest key."),
               kErrorToastMs);
    }
}

void EditorWindow::snapToGrid()
{
    if (!m_loaded) {
        return;
    }
    auto [first, last] = selection();
    if (first == last) {
        first = 0;
        last = std::max(0, m_document.lastColumn(m_grid));
    }
    if (m_document.snapColumns(first, last, m_grid)) {
        afterEdit();
        notify(ToastType::Success, trUtf8("Notes moved onto the grid."), kSavedToastMs);
    } else {
        notify(ToastType::Info, trUtf8("Every note is already on the grid."), kInfoToastMs);
    }
}

void EditorWindow::moveSelection(int delta)
{
    const auto [first, last] = selection();
    if (!m_loaded || first < 0) {
        return;
    }
    if (first + delta < 0) {
        notify(ToastType::Info, trUtf8("The notes are already at the start of the song."), kInfoToastMs);
        return;
    }
    if (!m_document.moveColumns(first, last, delta, m_grid)) {
        return;
    }
    refreshViews();
    m_anchor = (m_anchor < 0 ? m_cursor : m_anchor) + delta;
    m_cursor += delta;
    moveCursorTo(m_cursor, true);
}

void EditorWindow::adjustHold(int deltaColumns)
{
    const auto [first, last] = selection();
    if (m_loaded && first >= 0 && m_document.changeHold(first, last, deltaColumns, m_grid)) {
        afterEdit();
    }
}

void EditorWindow::dragColumn(int from, int to)
{
    if (!m_loaded || !m_document.moveColumns(from, from, to - from, m_grid)) {
        return;
    }
    refreshViews();
    moveCursorTo(to, false);
}

void EditorWindow::setAdvancedView(bool advanced)
{
    m_advanced = advanced;
    m_simpleView->setChecked(!advanced);
    m_advancedView->setChecked(advanced);
    m_views->setCurrentWidget(advanced ? static_cast<QWidget *>(m_timeline) : m_stepScroll);
    m_uiState.setEditorAdvancedView(advanced);
    if (m_loaded && m_cursor >= 0) {
        moveCursorTo(m_cursor, true);
    }
}

void EditorWindow::applySetup(bool advanced, bool colorful)
{
    setColorfulNotes(colorful);
    m_uiState.setEditorColorfulNotes(colorful);
    setAdvancedView(advanced);
    for (QWidget *widget : m_pad->findChildren<QWidget *>()) {
        widget->update();
    }
    m_timeline->update();
    m_steps->update();
    setFocus(Qt::OtherFocusReason);
}

void EditorWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_setupShown) {
        return;
    }
    m_setupShown = true;
    QPointer<EditorWindow> self(this);
    QTimer::singleShot(0, this, [self] {
        if (self) {
            self->m_setup->start(self->m_advanced, colorfulNotes());
        }
    });
}

void EditorWindow::refreshViews()
{
    m_timeline->documentChanged();
    m_steps->documentChanged();
    if (m_playing) {
        std::vector<SynthNote> notes;
        notes.reserve(m_document.notes().size());
        for (const EditorNote &note : m_document.notes()) {
            notes.push_back(SynthNote{note.time, note.key, note.hold});
        }
        m_audio->play(std::move(notes), m_audio->positionMs());
    }
}

void EditorWindow::pressColumn(int column, bool extend)
{
    moveCursorTo(column, extend);
    if (m_playing && !extend) {
        startPlayback(column);
    }
}

void EditorWindow::togglePlayback()
{
    if (m_playing) {
        stopPlayback();
        return;
    }
    const int first = selection().first;
    const int lastColumn = m_document.lastColumn(m_grid);
    startPlayback(first < 0 || first > lastColumn ? 0 : first);
}

void EditorWindow::startPlayback(int column)
{
    if (!m_loaded || m_document.empty()) {
        notify(ToastType::Info, trUtf8("Add some notes first, then press play."), kInfoToastMs);
        return;
    }
    const double from = static_cast<double>(m_grid.columnBegin(std::max(0, column)));
    std::vector<SynthNote> notes;
    notes.reserve(m_document.notes().size());
    for (const EditorNote &note : m_document.notes()) {
        notes.push_back(SynthNote{note.time, note.key, note.hold});
    }
    if (!m_audio->play(std::move(notes), from)) {
        stopPlayback();
        notify(ToastType::Warning, trUtf8("Nothing to play from here, or no sound output was found."), kErrorToastMs);
        return;
    }
    m_playing = true;
    m_playColumn = -1;
    m_play->setIconId(QStringLiteral("pause-fill"));
    m_play->setToolTip(tr("Pause (Space)"));
    m_playTimer->start();
    m_idleTimer->start();
}

void EditorWindow::stopPlayback()
{
    m_audio->stop();
    m_playing = false;
    m_playColumn = -1;
    m_playTimer->stop();
    m_timeline->setPlayhead(-1);
    m_steps->setPlayhead(-1);
    m_play->setIconId(QStringLiteral("play-fill"));
    m_play->setToolTip(tr("Play from the selected column (Space)"));
}

void EditorWindow::playbackTick()
{
    if (!m_audio->isPlaying()) {
        stopPlayback();
        return;
    }
    const double position = m_audio->positionMs();
    m_timeline->setPlayhead(position);
    m_steps->setPlayhead(position);
    const int column = m_grid.columnOf(position);
    if (column == m_playColumn) {
        return;
    }
    m_playColumn = column;
    moveCursorTo(column, false);
    for (int key : m_document.keysInColumn(column, m_grid)) {
        m_pad->flash(key);
    }
}

void EditorWindow::toggleSound()
{
    m_audio->setMuted(!m_audio->muted());
    m_uiState.setEditorSoundMuted(m_audio->muted());
    refreshSoundButton();
}

void EditorWindow::refreshSoundButton()
{
    const bool muted = m_audio->muted();
    const QString source = m_audio->hasSamples() ? tr("Sky piano") : tr("built-in sound");
    m_sound->setIconId(muted ? QStringLiteral("volume-mute") : QStringLiteral("volume-up"));
    m_sound->setToolTip(muted ? tr("Sound is off. Click to turn it on.")
                              : tr("Playing with the %1. Click to mute.").arg(source));
}

void EditorWindow::loadSounds()
{
    QPointer<EditorWindow> self(this);
    const std::filesystem::path folder = m_soundFolder;
    const int rate = m_audio->sampleRate();
    QThreadPool::globalInstance()->start([self, folder, rate] {
        auto bank = folder.empty() ? Result<SampleBank>(Error{"SOUND_FOLDER_MISSING", {}, {}})
                                   : loadSampleBank(folder, rate);
        if (!bank) {
            bank = loadBuiltInSoundpack(kSoundpackInstrument, rate);
        }
        if (!bank) {
            Logger::info("editor", bank.error().details);
            return;
        }
        auto shared = std::make_shared<const SampleBank>(std::move(bank).value());
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, shared] {
            if (self) {
                self->m_audio->setSamples(shared);
                self->refreshSoundButton();
            }
        });
    });
}

void EditorWindow::zoomViews(int steps)
{
    if (m_advanced) {
        m_timeline->zoomBy(steps);
    } else {
        m_steps->zoomBy(steps);
        moveCursorTo(m_cursor, true);
    }
}

int EditorWindow::pageColumns() const
{
    if (m_advanced) {
        return m_timeline->visibleColumns();
    }
    const int rows = std::max(1, m_stepScroll->viewport()->height() /
                                     std::max(1, m_steps->tileBounds(0).height()));
    return rows * m_steps->columnsPerRow();
}

void EditorWindow::beginEditing(int field)
{
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
    const bool bpmField = m_editingField == MetaPanel::Bpm;
    OrderedJson value;
    if (bpmField) {
        const auto bpm = parseLeadingInt(text);
        if (!bpm || *bpm <= 0) {
            notify(ToastType::Error, trUtf8("Please enter a valid BPM number (must be greater than 0)"), kErrorToastMs);
            cancelEditing();
            return;
        }
        value = *bpm;
    } else {
        value = fromQString(text);
    }
    if (auto saved = m_library.updateEntryField(index, kFieldKeys[static_cast<std::size_t>(m_editingField)], value);
        !saved) {
        notify(ToastType::Error, trUtf8("Failed to save changes to the sheet"), kErrorToastMs);
        cancelEditing();
        return;
    }
    field->setValue(value.is_string() ? toQString(value.get<std::string>()) : toQString(formatJsNumber(value.get<double>())));
    notify(ToastType::Success, trUtf8("Changes saved successfully!"), kSavedToastMs);
    cancelEditing();
    refreshHeader();
    if (bpmField && m_loaded) {
        m_steady = true;
        setGrid(TimeGrid(sheetBpm(), m_grid.subdivision(), m_grid.originMs()));
    }
}

void EditorWindow::saveNotes()
{
    if (!m_loaded || !m_document.isDirty()) {
        return;
    }
    if (m_document.empty()) {
        notify(ToastType::Warning, trUtf8("Add at least one note before saving."), kErrorToastMs);
        return;
    }
    auto payload = payloadFromSongNotes(m_document.songNotes());
    if (!payload) {
        notify(ToastType::Error, trUtf8("Failed to save changes to the music sheet"), kErrorToastMs);
        Logger::error("editor", payload.error().details);
        return;
    }
    if (auto saved = m_library.saveEditedNotes(libraryIndex(), payload.value(), m_document.hasHoldNotes()); !saved) {
        notify(ToastType::Error, trUtf8("Failed to save changes to the music sheet"), kErrorToastMs);
        return;
    }
    m_document.markSaved();
    notify(ToastType::Success, trUtf8("Sheet saved successfully!"), kSavedToastMs);
    refreshActions();
}

void EditorWindow::onSave()
{
    if (m_editingField >= 0) {
        commitEditing();
    } else {
        saveNotes();
    }
}

void EditorWindow::onCancel()
{
    if (m_editingField >= 0) {
        cancelEditing();
        return;
    }
    if (!m_document.isDirty()) {
        return;
    }
    QPointer<EditorWindow> self(this);
    m_confirm->ask(tr(kDiscardPrompt), [self] {
        if (self) {
            self->m_document.discard();
            self->afterEdit();
        }
    });
}

void EditorWindow::exportSheet()
{
    const SheetEntry *row = m_library.entry(libraryIndex());
    if (!row || !m_loaded || m_document.empty()) {
        notify(ToastType::Warning, trUtf8("No notes to export."), kErrorToastMs);
        return;
    }
    auto payload = payloadFromSongNotes(m_document.songNotes());
    if (!payload) {
        notify(ToastType::Error, trUtf8("The sheet could not be exported."), kErrorToastMs);
        Logger::error("editor", payload.error().details);
        return;
    }
    const OrderedJson document = buildExportDocument(*row, payload.value().songNotes, payload.value().keyMap);
    const QString name = row->name().empty() ? tr("Sheet") : toQString(row->name());
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
    const bool dirty = m_document.isDirty();
    const bool ready = m_loaded && m_cursor >= 0;
    m_undo->setEnabled(m_loaded && m_document.canUndo());
    m_redo->setEnabled(m_loaded && m_document.canRedo());
    for (QAbstractButton *button :
         {m_insert, m_delete, m_erase, m_copy, m_cut, m_keyUp, m_keyDown, m_earlier, m_later, m_holdLess, m_holdMore}) {
        button->setEnabled(ready);
    }
    m_paste->setEnabled(ready && !m_clip.empty());
    m_snap->setEnabled(m_loaded && !m_document.empty());
    m_save->setEnabled(m_loaded && (dirty || m_editingField >= 0));
    m_cancel->setEnabled(dirty || m_editingField >= 0);
    m_export->setEnabled(m_loaded && !m_document.empty());
    m_saveState->setText(!m_loaded ? QString()
                                   : (dirty ? tr("Unsaved changes") : tr("All changes saved")));
    m_saveState->setProperty("dirty", dirty);
    m_saveState->style()->unpolish(m_saveState);
    m_saveState->style()->polish(m_saveState);
    setWindowModified(dirty);
    refreshStatus();
}

void EditorWindow::refreshStatus()
{
    if (!m_loaded) {
        m_status->setText(tr("Loading the sheet…"));
        m_selectionTitle->setText(QStringLiteral("—"));
        m_selectionDetail->clear();
        m_hint->setVisible(false);
        return;
    }
    const auto [first, last] = selection();
    if (first >= 0 && first != last) {
        m_selectionTitle->setText(tr("Columns %1 – %2").arg(first + 1).arg(last + 1));
        const auto range = m_document.indexRange(m_grid.columnBegin(first), m_grid.columnBegin(last + 1));
        const int notes = range.second - range.first;
        m_selectionDetail->setText(tr("%1 to %2\n%3 in %4")
                                       .arg(clockText(static_cast<double>(m_grid.timeOf(first))),
                                            clockText(static_cast<double>(m_grid.timeOf(last))),
                                            countText(notes, trNoop("1 note"), trNoop("%1 notes")),
                                            countText(last - first + 1, trNoop("1 column"), trNoop("%1 columns"))));
    } else if (m_cursor >= 0) {
        const int bar = m_cursor / (m_grid.subdivision() * kBeatsPerBar) + 1;
        const int beat = (m_cursor / m_grid.subdivision()) % kBeatsPerBar + 1;
        m_selectionTitle->setText(tr("Column %1").arg(m_cursor + 1));
        const std::vector<int> keys = m_document.keysInColumn(m_cursor, m_grid);
        QStringList names;
        for (int key : keys) {
            const std::string_view name = kAutoKeys[static_cast<std::size_t>(key)];
            names << QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size())).toUpper();
        }
        QString notesLine = keys.empty() ? tr("Rest. Click a key below to add a note.")
                                         : tr("Notes: %1").arg(names.join(QStringLiteral("  ")));
        if (const std::int64_t hold = m_document.longestHold(m_cursor, m_cursor, m_grid); hold > 0) {
            const long long columns = std::max(1LL, std::llround(static_cast<double>(hold) / m_grid.slotMs()));
            notesLine += tr("\nHeld for %1").arg(countText(static_cast<int>(columns), trNoop("1 column"), trNoop("%1 columns")));
        }
        m_selectionDetail->setText(tr("%1  ·  bar %2, beat %3\n%4")
                                       .arg(clockText(static_cast<double>(m_grid.timeOf(m_cursor))))
                                       .arg(bar)
                                       .arg(beat)
                                       .arg(notesLine));
    }
    m_hint->setVisible(!m_steady && !m_document.empty() && m_hintRoom);
    m_hint->setText(tr("No steady beat: this sheet was probably recorded live, so notes sit at their "
                                   "exact times. Snap to grid lines them up."));

    QStringList parts;
    if (!m_side->isVisibleTo(this)) {
        parts << selectionSummary();
    }
    parts << tr("%1, %2 long").arg(countText(static_cast<int>(m_document.notes().size()), trNoop("1 note"), trNoop("%1 notes")),
                                                clockText(static_cast<double>(m_document.endTime())));
    if (m_steady) {
        parts << tr("Tempo %1 BPM").arg(std::llround(m_grid.bpm()));
    } else {
        parts << tr("No steady beat");
    }
    parts << tr("1 column = %1 ms").arg(m_grid.slotMs(), 0, 'f', 0);
    if (const int offGrid = m_document.offGridCount(m_grid); m_steady && offGrid > 0) {
        parts << tr("%1 between columns").arg(countText(offGrid, trNoop("1 note"), trNoop("%1 notes")));
    }
    m_status->setText(parts.join(QStringLiteral("   ·   ")));
}

QString EditorWindow::selectionSummary() const
{
    const auto [first, last] = selection();
    if (first < 0) {
        return QString();
    }
    if (first != last) {
        return tr("Columns %1 – %2").arg(first + 1).arg(last + 1);
    }
    QStringList names;
    for (int key : m_document.keysInColumn(m_cursor, m_grid)) {
        const std::string_view name = kAutoKeys[static_cast<std::size_t>(key)];
        names << QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size())).toUpper();
    }
    return tr("Column %1 at %2: %3")
        .arg(m_cursor + 1)
        .arg(clockText(static_cast<double>(m_grid.timeOf(m_cursor))))
        .arg(names.isEmpty() ? tr("rest") : names.join(QLatin1Char(' ')));
}

void EditorWindow::refreshButtonIcons()
{
    const Theme &theme = currentTheme();
    const QSize size(kButtonIcon, kButtonIcon);
    m_save->setIcon(IconCache::icon(QStringLiteral("floppy"), kButtonIcon, theme.editor.actionText));
    m_cancel->setIcon(IconCache::icon(QStringLiteral("arrow-counterclockwise"), kButtonIcon, theme.editor.actionText));
    m_export->setIcon(IconCache::icon(QStringLiteral("box-arrow-up"), kButtonIcon, theme.editor.exportText));
    for (QPushButton *button : {m_save, m_cancel, m_export}) {
        button->setIconSize(size);
    }
}

void EditorWindow::refreshHeader()
{
    const SheetEntry *row = m_library.entry(libraryIndex());
    if (!row) {
        m_fullTitle = tr("Sheet Editor");
        elideTitle();
        m_subtitle->clear();
        return;
    }
    const QString name = row->name().empty() ? tr("Untitled") : toQString(row->name());
    m_fullTitle = name;
    elideTitle();
    setWindowTitle(tr("%1[*] — Sheet Editor").arg(name));
    QStringList details;
    details << tr("by %1").arg(row->author().empty() ? tr("Unknown") : toQString(row->author()));
    const double bpm = row->bpm();
    if (std::isfinite(bpm) && bpm > 0) {
        details << tr("%1 BPM").arg(toQString(formatJsNumber(bpm)));
    }
    m_subtitle->setText(details.join(QStringLiteral("  ·  ")));
}

void EditorWindow::notify(ToastType type, const std::string &message, int durationMs)
{
    m_toasts->show(Toast{type, {}, message, durationMs});
}

void EditorWindow::closeEvent(QCloseEvent *event)
{
    if (m_closeConfirmed || (!m_document.isDirty() && m_editingField < 0)) {
        QWidget::closeEvent(event);
        return;
    }
    event->ignore();
    QPointer<EditorWindow> self(this);
    m_confirm->ask(tr(kDiscardPrompt), [self] {
        if (self) {
            self->m_closeConfirmed = true;
            self->close();
        }
    });
}

void EditorWindow::keyPressEvent(QKeyEvent *event)
{
    if (m_editingField >= 0) {
        QWidget::keyPressEvent(event);
        return;
    }
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    const bool shift = modifiers.testFlag(Qt::ShiftModifier);
    const bool control = modifiers.testFlag(Qt::ControlModifier);
    const int key = event->key();
    const int bar = m_grid.subdivision() * kBeatsPerBar;

    if (key == Qt::Key_Escape) {
        if (m_playing) {
            stopPlayback();
            return;
        }
        moveCursorTo(m_cursor, false);
        return;
    }
    if (key == Qt::Key_Space && !(modifiers & (Qt::ControlModifier | Qt::AltModifier))) {
        if (!event->isAutoRepeat()) {
            togglePlayback();
        }
        return;
    }
    if (event->matches(QKeySequence::Save)) {
        saveNotes();
        return;
    }
    if (event->matches(QKeySequence::Undo)) {
        undoEdit();
        return;
    }
    if (event->matches(QKeySequence::Redo) || (control && shift && key == Qt::Key_Z)) {
        redoEdit();
        return;
    }
    if (event->matches(QKeySequence::Copy)) {
        copyColumns(false);
        return;
    }
    if (event->matches(QKeySequence::Cut)) {
        copyColumns(true);
        return;
    }
    if (control && key == Qt::Key_V) {
        pasteColumns(!shift);
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        m_anchor = 0;
        m_cursor = 0;
        moveCursorTo(std::max(0, m_document.lastColumn(m_grid)), true);
        return;
    }
    if (control && key == Qt::Key_D) {
        duplicateColumns();
        return;
    }
    if (control && (key == Qt::Key_Equal || key == Qt::Key_Plus)) {
        zoomViews(1);
        return;
    }
    if (control && key == Qt::Key_Minus) {
        zoomViews(-1);
        return;
    }
    if (modifiers.testFlag(Qt::AltModifier) && !control && (key == Qt::Key_Left || key == Qt::Key_Right)) {
        moveSelection(key == Qt::Key_Left ? -1 : 1);
        return;
    }
    if (modifiers.testFlag(Qt::AltModifier) && !control && (key == Qt::Key_Up || key == Qt::Key_Down)) {
        shiftKeys(key == Qt::Key_Up ? 1 : -1);
        return;
    }
    if (control && (key == Qt::Key_Left || key == Qt::Key_Right)) {
        moveCursorTo(m_cursor + (key == Qt::Key_Left ? -bar : bar), shift);
        return;
    }
    if (modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        QWidget::keyPressEvent(event);
        return;
    }
    switch (key) {
    case Qt::Key_Left:
    case Qt::Key_A:
        moveCursorTo(m_cursor - 1, shift);
        return;
    case Qt::Key_Right:
    case Qt::Key_D:
        moveCursorTo(m_cursor + 1, shift);
        return;
    case Qt::Key_Home:
        moveCursorTo(0, shift);
        return;
    case Qt::Key_End:
        moveCursorTo(std::max(0, m_document.lastColumn(m_grid)), shift);
        return;
    case Qt::Key_Up:
    case Qt::Key_Down:
        if (!m_advanced) {
            moveCursorTo(m_cursor + (key == Qt::Key_Up ? -1 : 1) * m_steps->columnsPerRow(), shift);
            return;
        }
        break;
    case Qt::Key_PageUp:
        moveCursorTo(m_cursor - pageColumns(), shift);
        return;
    case Qt::Key_PageDown:
        moveCursorTo(m_cursor + pageColumns(), shift);
        return;
    case Qt::Key_BracketLeft:
        adjustHold(-1);
        return;
    case Qt::Key_BracketRight:
        adjustHold(1);
        return;
    case Qt::Key_E:
    case Qt::Key_Insert:
        insertColumn();
        return;
    case Qt::Key_Q:
    case Qt::Key_Delete:
        deleteColumns();
        return;
    case Qt::Key_Backspace:
        eraseColumns();
        return;
    default:
        break;
    }
    const int index = autoKeyIndex(fromQString(event->text().toLower()));
    if (index >= 0 && !event->isAutoRepeat()) {
        toggleKey(index, true);
        return;
    }
    QWidget::keyPressEvent(event);
}

}  // namespace sky
