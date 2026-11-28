#include "ui/main_window/playlist_picker.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "core/translator.hpp"
#include "core/string_utils.hpp"
#include "services/library/library_service.hpp"
#include "services/library/playlist_service.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/icon_button.hpp"

namespace sky {

namespace {

constexpr int kMaxWidth = 460;
constexpr int kMaxHeight = 560;
constexpr int kOuterPadding = 20;
constexpr int kPadding = 18;
constexpr qreal kRadius = 12;

}  // namespace

PlaylistPicker::PlaylistPicker(PlaylistService &playlists, LibraryService &library, QWidget *window)
    : Overlay(window),
      m_playlists(playlists),
      m_library(library),
      m_panel(new QWidget(this)),
      m_title(new QLabel(m_panel)),
      m_close(new IconButton(QStringLiteral("x"), 16, QSize(28, 28),
                             IconButtonStyle{[] { return currentTheme().fontColor; }, [] { return currentTheme().hoverColor; },
                                             {}, {}, 6, 0, kHoverMs},
                             m_panel)),
      m_name(new QLineEdit(m_panel)),
      m_submit(new QPushButton(m_panel)),
      m_scroll(new QScrollArea(m_panel)),
      m_rows(new QWidget()),
      m_rowsLayout(new QVBoxLayout(m_rows))
{
    m_panel->setObjectName(QStringLiteral("PickerPanel"));
    m_title->setObjectName(QStringLiteral("PickerTitle"));
    m_name->setObjectName(QStringLiteral("PickerNameField"));
    m_name->setMaxLength(static_cast<int>(kPlaylistNameMaxLength));
    m_submit->setObjectName(QStringLiteral("PrimaryButton"));
    m_submit->setCursor(Qt::PointingHandCursor);
    m_close->setToolTip(tr("Close"));
    m_scroll->setObjectName(QStringLiteral("PickerList"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_rows->setObjectName(QStringLiteral("PickerRows"));
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(0);
    m_scroll->setWidget(m_rows);

    auto *header = new QHBoxLayout();
    header->setSpacing(10);
    header->addWidget(m_title, 1);
    header->addWidget(m_close);

    auto *create = new QHBoxLayout();
    create->setSpacing(10);
    create->addWidget(m_name, 1);
    create->addWidget(m_submit);

    auto *column = new QVBoxLayout(m_panel);
    column->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    column->setSpacing(0);
    column->addLayout(header);
    column->addSpacing(16);
    column->addLayout(create);
    column->addSpacing(14);
    column->addWidget(m_scroll, 1);

    connect(m_close, &QAbstractButton::clicked, this, &Overlay::close);
    connect(m_submit, &QPushButton::clicked, this, &PlaylistPicker::submit);
    connect(m_name, &QLineEdit::returnPressed, this, &PlaylistPicker::submit);
    connect(&m_playlists, &PlaylistService::changed, this, [this] {
        if (isOpen()) {
            renderRows();
        }
    });
}

QWidget *PlaylistPicker::panel() const
{
    return m_panel;
}

QColor PlaylistPicker::dimColor() const
{
    return currentTheme().overlayDim;
}

ShadowSpec PlaylistPicker::panelShadow() const
{
    return currentTheme().shadowPicker;
}

qreal PlaylistPicker::panelRadius() const
{
    return kRadius;
}

QRect PlaylistPicker::panelGeometry() const
{
    const int width = std::min(kMaxWidth, this->width() - kOuterPadding * 2);
    const int limit = std::min(kMaxHeight, static_cast<int>(this->height() * 0.9));
    const int wanted = m_panel->sizeHint().height() + m_rows->sizeHint().height();
    const int height = std::clamp(wanted, std::min(160, limit), limit);
    return QRect((this->width() - width) / 2, (this->height() - height) / 2, width, height);
}

void PlaylistPicker::paintPanel(QPainter &painter, const QRectF &rect)
{
    const Theme &theme = currentTheme();
    const QPainterPath path = roundedPath(rect.adjusted(0.5, 0.5, -0.5, -0.5), kRadius);
    painter.fillPath(path, theme.background);
    painter.setPen(QPen(theme.pickerBorder, 1));
    painter.drawPath(path);
}

void PlaylistPicker::present(const QString &title, const QString &submitLabel, const QString &name)
{
    m_title->setText(title);
    m_submit->setText(submitLabel);
    m_name->setText(name);
    renderRows();
    open();
    QTimer::singleShot(0, this, [this] {
        m_name->setFocus();
        m_name->selectAll();
    });
}

void PlaylistPicker::openForSheet(int libraryIndex)
{
    if (!m_library.entry(libraryIndex)) {
        return;
    }
    m_sheetIndex = libraryIndex;
    m_renameId.clear();
    present(tr("Add to Playlist"), tr("Create"), {});
}

void PlaylistPicker::openForNew()
{
    m_sheetIndex = -1;
    m_renameId.clear();
    present(tr("New Playlist"), tr("Create"), {});
}

void PlaylistPicker::openForRename(const std::string &playlistId)
{
    const Playlist *playlist = m_playlists.find(playlistId);
    if (!playlist) {
        return;
    }
    m_sheetIndex = -1;
    m_renameId = playlistId;
    present(tr("Rename Playlist"), tr("Save"), toQString(playlist->name));
}

void PlaylistPicker::addTo(const std::string &playlistId)
{
    const SheetEntry *entry = m_library.entry(m_sheetIndex);
    if (!entry || !m_playlists.find(playlistId)) {
        return;
    }
    if (auto added = m_playlists.addSheet(playlistId, entry->payloadFile()); !added) {
        emit toastRequested(Toast{ToastType::Warning, {}, added.error().userMessage, kDefaultToastMs});
        return;
    }
    renderRows();
    emit toastRequested(Toast{ToastType::Success, {}, trUtf8("Added to playlist."), kDefaultToastMs});
}

void PlaylistPicker::submit()
{
    if (!m_renameId.empty()) {
        if (auto renamed = m_playlists.rename(m_renameId, fromQString(m_name->text())); !renamed) {
            emit toastRequested(Toast{ToastType::Warning, {}, renamed.error().userMessage, kDefaultToastMs});
            return;
        }
        close();
        return;
    }
    auto created = m_playlists.create(fromQString(m_name->text()));
    if (!created) {
        emit toastRequested(Toast{ToastType::Warning, {}, created.error().userMessage, kDefaultToastMs});
        return;
    }
    m_name->clear();
    emit playlistCreated(created.value().id);
    if (m_sheetIndex >= 0) {
        addTo(created.value().id);
    } else {
        close();
    }
    renderRows();
}

void PlaylistPicker::renderRows()
{
    while (QLayoutItem *item = m_rowsLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    const SheetEntry *sheet = m_library.entry(m_sheetIndex);
    m_scroll->setVisible(sheet != nullptr);
    if (sheet) {
        if (m_playlists.playlists().empty()) {
            auto *empty = new QLabel(tr("Create a playlist to add %1.").arg(toQString(sheet->name())), m_rows);
            empty->setObjectName(QStringLiteral("PickerEmpty"));
            empty->setWordWrap(true);
            m_rowsLayout->addWidget(empty);
        }
        for (const Playlist &playlist : m_playlists.playlists()) {
            const bool added = std::find(playlist.sheetKeys.begin(), playlist.sheetKeys.end(), sheet->payloadFile()) !=
                               playlist.sheetKeys.end();
            auto *row = new QWidget(m_rows);
            row->setObjectName(QStringLiteral("PickerRow"));
            row->setAttribute(Qt::WA_StyledBackground);
            auto *layout = new QHBoxLayout(row);
            layout->setContentsMargins(0, 10, 0, 10);
            auto *name = new QLabel(toQString(playlist.name), row);
            auto *action = new QPushButton(added ? tr("Added") : tr("Add"), row);
            action->setObjectName(QStringLiteral("PrimaryButton"));
            action->setEnabled(!added);
            action->setCursor(Qt::PointingHandCursor);
            const std::string id = playlist.id;
            connect(action, &QPushButton::clicked, this, [this, id] { addTo(id); });
            layout->addWidget(name, 1);
            layout->addWidget(action);
            m_rowsLayout->addWidget(row);
        }
    }
    m_rowsLayout->addStretch();
    relayout();
}

}  // namespace sky
