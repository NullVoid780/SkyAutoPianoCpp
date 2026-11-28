#include "ui/main_window/store_view.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

#include "core/translator.hpp"
#include "ui/controllers/player_controller.hpp"
#include "core/string_utils.hpp"
#include "services/library/library_service.hpp"
#include "services/store/store_service.hpp"
#include "ui/main_window/store_card_delegate.hpp"
#include "ui/main_window/store_pagination.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/state_card.hpp"

namespace sky {

namespace {

constexpr int kLimit = 20;
constexpr int kDebounceMs = 300;
constexpr int kSidePadding = 20;
constexpr int kBannerPadY = 6;

QString metaLine(const StoreItem &item)
{
    QStringList parts;
    if (item.author != "Unknown") {
        parts << toQString(item.author);
    }
    if (item.transcribedBy != "Unknown") {
        parts << tr("Transcribed by %1").arg(toQString(item.transcribedBy));
    }
    if (item.creator && !item.creator->displayName.empty()) {
        parts << tr("Uploaded by %1").arg(toQString(item.creator->displayName));
    }
    return parts.isEmpty() ? tr("Unknown") : parts.join(QStringLiteral(" · "));
}

QStringList badgesFor(const StoreItem &item)
{
    QStringList badges;
    badges << (item.difficulty.empty() ? tr("Unknown") : toQString(item.difficulty));
    badges << (item.bpm ? tr("%1 BPM").arg(item.bpm) : tr("Unknown BPM"));
    badges << (item.noteCount ? tr("%1 notes").arg(item.noteCount) : tr("Unknown notes"));
    if (item.accessMode == "web_only") {
        badges << tr("Web only");
    }
    if (item.priceAmount > 0) {
        badges << tr("Paid");
    }
    for (std::size_t i = 0; i < item.tags.size() && i < 3; ++i) {
        badges << toQString(item.tags[i]);
    }
    return badges;
}

}  // namespace

StoreView::StoreView(StoreService &store, LibraryService &library, PlayerController &player, QWidget *parent)
    : QWidget(parent),
      m_store(store),
      m_library(library),
      m_player(player),
      m_model(new QStandardItemModel(this)),
      m_delegate(new StoreCardDelegate(this)),
      m_list(new QListView(this)),
      m_state(new StateCard(this)),
      m_retry(new QPushButton(tr("Retry"), this)),
      m_pagination(new StorePagination(this)),
      m_debounce(new QTimer(this))
{
    auto *title = new QLabel(tr("Sky Sheet Store"), this);
    title->setObjectName(QStringLiteral("LibraryHeading"));
    auto *subtitle = new QLabel(tr("Browse community sheets from skysheet.store"), this);
    subtitle->setObjectName(QStringLiteral("StoreSubtitle"));
    auto *refresh = new QPushButton(tr("Refresh"), this);
    refresh->setObjectName(QStringLiteral("NeutralButton"));
    refresh->setCursor(Qt::PointingHandCursor);
    m_retry->setObjectName(QStringLiteral("NeutralButton"));
    m_retry->setCursor(Qt::PointingHandCursor);

    auto *copy = new QVBoxLayout();
    copy->setSpacing(4);
    copy->addWidget(title);
    copy->addWidget(subtitle);
    auto *banner = new QHBoxLayout();
    banner->setContentsMargins(kSidePadding, kBannerPadY, kSidePadding, kBannerPadY);
    banner->addLayout(copy, 1);
    banner->addWidget(refresh, 0, Qt::AlignVCenter);

    m_list->setObjectName(QStringLiteral("LibraryList"));
    m_list->setModel(m_model);
    m_list->setItemDelegate(m_delegate);
    m_list->setMouseTracking(true);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->viewport()->installEventFilter(this);

    auto *retryRow = new QHBoxLayout();
    retryRow->setContentsMargins(kSidePadding + 18, 0, kSidePadding, 0);
    retryRow->addWidget(m_retry);
    retryRow->addStretch();

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addLayout(banner);
    column->addWidget(m_state);
    column->addLayout(retryRow);
    column->addWidget(m_list, 1);
    column->addWidget(m_pagination);
    column->addStretch(0);

    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, [this] { fetch(1, false); });
    connect(refresh, &QPushButton::clicked, this, [this] { fetch(1, true); });
    connect(m_retry, &QPushButton::clicked, this, [this] { fetch(1, true); });
    connect(m_pagination, &StorePagination::pageRequested, this, [this](int page) {
        if (page >= 1 && page <= totalPages() && page != m_page) {
            fetch(page, false);
        }
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, m_list->viewport(), [this] {
        m_list->doItemsLayout();
        m_list->viewport()->update();
    });
    render();
}

void StoreView::activate()
{
    if (!m_hasLoaded && !m_loading) {
        fetch(1, false);
    }
}

void StoreView::setQuery(const QString &query)
{
    const QString trimmed = query.trimmed();
    if (trimmed == m_query && m_hasLoaded) {
        return;
    }
    m_query = trimmed;
    m_debounce->start();
}

int StoreView::totalPages() const
{
    return std::max(1, static_cast<int>(std::ceil(static_cast<double>(m_total) / kLimit)));
}

const StoreItem *StoreView::itemById(const std::string &id) const
{
    for (const StoreItem &item : m_items) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}

void StoreView::fetch(int page, bool refresh)
{
    const int safePage = page > 0 ? page : 1;
    const bool initial = safePage == 1;
    if (initial) {
        ++m_generation;
        m_loading = true;
        m_loadingPage = false;
        m_error.clear();
    } else {
        if (m_loading || m_loadingPage) {
            return;
        }
        m_loadingPage = true;
    }
    render();

    const quint64 generation = m_generation;
    const QString query = m_query;
    StoreQuery request;
    request.q = fromQString(query);
    request.limit = kLimit;
    request.offset = static_cast<std::int64_t>(safePage - 1) * kLimit;
    request.refresh = refresh;
    QPointer<StoreView> self(this);
    m_store.listSheets(request, [self, generation, query, safePage, initial](Result<StorePage> result) {
        if (!self || generation != self->m_generation || query != self->m_query) {
            return;
        }
        if (result) {
            self->m_items = result.value().items;
            self->m_page = safePage;
            self->m_total = result.value().total;
            self->m_hasLoaded = true;
        } else if (initial) {
            self->m_error = toQString(result.error().userMessage);
        } else {
            const std::string message = result.error().userMessage.empty() ? trUtf8("Unable to load this page. Please try again.")
                                                                             : result.error().userMessage;
            emit self->toastRequested(Toast{ToastType::Error, {}, message, kDefaultToastMs});
        }
        if (initial) {
            self->m_loading = false;
        }
        self->m_loadingPage = false;
        self->render();
        if (result) {
            self->m_list->scrollToTop();
        }
    });
}

void StoreView::render()
{
    const bool empty = m_items.empty();
    if (m_loading && empty) {
        m_state->setContent(tr("Loading sheets..."), {});
    } else if (!m_error.isEmpty() && empty) {
        m_state->setContent(tr("Unable to connect to Sky Sheet Store."), m_error);
    } else if (empty) {
        m_state->setContent(tr("No sheets found."), {});
    }
    m_state->setVisible(empty);
    m_retry->setVisible(empty && !m_loading && !m_error.isEmpty());
    m_list->setVisible(!empty);

    m_model->clear();
    for (const StoreItem &item : m_items) {
        const StoreAvailability availability = storeAvailability(item);
        const bool playing = m_playingId == item.id;
        const bool saving = m_savingId == item.id;
        auto *row = new QStandardItem();
        row->setEditable(false);
        row->setData(toQString(item.title), StoreCardDelegate::TitleRole);
        row->setData(metaLine(item), StoreCardDelegate::MetaRole);
        row->setData(QStringLiteral("%1. %2").arg(toQString(item.title), metaLine(item)), Qt::AccessibleTextRole);
        row->setData(badgesFor(item), StoreCardDelegate::BadgesRole);
        row->setData(toQString(availability.message), StoreCardDelegate::StatusRole);
        row->setData(availability.canPlay && !playing, StoreCardDelegate::CanPlayRole);
        row->setData(availability.canSave && !saving, StoreCardDelegate::CanSaveRole);
        row->setData(playing ? tr("Preparing...") : tr("Play"), StoreCardDelegate::PlayLabelRole);
        row->setData(saving ? tr("Saving...") : tr("Save Sheet"), StoreCardDelegate::SaveLabelRole);
        row->setData(toQString(item.id), StoreCardDelegate::IdRole);
        m_model->appendRow(row);
    }
    m_pagination->setPages(m_page, empty ? 1 : totalPages(), m_loadingPage);
}

void StoreView::prepare(const StoreItem &item, std::function<void(Result<PreparedStoreSheet>)> done)
{
    if (const auto cached = m_prepared.find(item.id); cached != m_prepared.end()) {
        done(cached->second);
        return;
    }
    QPointer<StoreView> self(this);
    const StoreItem snapshot = item;
    m_store.getPlayerSheet(item.id, item.sourceType.empty() ? "user" : item.sourceType,
                           [self, snapshot, done](Result<StorePlayerPayload> payload) {
                               if (!self) {
                                   return;
                               }
                               if (!payload) {
                                   done(payload.error());
                                   return;
                               }
                               auto prepared = prepareStoreSheet(payload.value(), snapshot);
                               if (prepared) {
                                   self->m_prepared[snapshot.id] = prepared.value();
                               }
                               done(std::move(prepared));
                           });
}

void StoreView::play(const std::string &id)
{
    const StoreItem *item = itemById(id);
    if (!item) {
        return;
    }
    const StoreAvailability availability = storeAvailability(*item);
    if (!availability.canPlay) {
        emit toastRequested(Toast{ToastType::Error, {},
                                  availability.message.empty() ? trUtf8("This sheet is not available for playback.") : availability.message,
                                  kDefaultToastMs});
        return;
    }
    m_playingId = id;
    render();
    QPointer<StoreView> self(this);
    const StoreItem snapshot = *item;
    prepare(snapshot, [self, snapshot](Result<PreparedStoreSheet> prepared) {
        if (!self) {
            return;
        }
        if (prepared) {
            if (self->m_player.isPlaying()) {
                self->m_player.togglePlay();
            }
            const OrderedJson &sheet = prepared.value().normalizedSheet;
            self->m_player.setRemoteTarget(sheet.value("name", std::string()), prepared.value().keyMap,
                                           prepared.value().playbackNotes);
            self->m_player.togglePlay();
            self->m_store.recordPlayerStart(snapshot.id, snapshot.sourceType.empty() ? "user" : snapshot.sourceType);
        } else {
            const std::string message = prepared.error().userMessage.empty() ? trUtf8("Unable to load this sheet. Please try again.")
                                                                               : prepared.error().userMessage;
            emit self->toastRequested(Toast{ToastType::Error, {}, message, kDefaultToastMs});
        }
        self->m_playingId.clear();
        self->render();
    });
}

void StoreView::save(const std::string &id)
{
    const StoreItem *item = itemById(id);
    if (!item) {
        return;
    }
    const StoreAvailability availability = storeAvailability(*item);
    if (!availability.canSave) {
        emit toastRequested(Toast{ToastType::Error, {},
                                  availability.message.empty() ? trUtf8("This sheet cannot be saved.") : availability.message,
                                  kDefaultToastMs});
        return;
    }
    m_savingId = id;
    render();
    QPointer<StoreView> self(this);
    const StoreItem snapshot = *item;
    prepare(snapshot, [self, snapshot](Result<PreparedStoreSheet> prepared) {
        if (!self) {
            return;
        }
        const auto finish = [self] {
            if (self) {
                self->m_savingId.clear();
                self->render();
            }
        };
        if (!prepared) {
            const std::string message = prepared.error().userMessage.empty() ? trUtf8("Unable to load this sheet. Please try again.")
                                                                               : prepared.error().userMessage;
            emit self->toastRequested(Toast{ToastType::Error, {}, message, kDefaultToastMs});
            finish();
            return;
        }
        const StoreSource source{"sky-sheet-store", snapshot.id, snapshot.sourceUrl};
        self->m_library.importStoreSheet(prepared.value().normalizedSheet, source, [self, snapshot, finish](Result<void> saved) {
            if (!self) {
                return;
            }
            if (saved) {
                emit self->toastRequested(Toast{ToastType::Success, {}, trUtf8("Saved to Library."), kDefaultToastMs});
                self->m_store.recordDownload(snapshot.id, snapshot.sourceType.empty() ? "user" : snapshot.sourceType);
            } else {
                emit self->toastRequested(Toast{ToastType::Error, {}, saved.error().userMessage, kDefaultToastMs});
            }
            finish();
        });
    });
}

void StoreView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_delegate->setViewWidth(m_list->viewport()->width());
}

bool StoreView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_list->viewport()) {
        return false;
    }
    const auto locate = [this](const QPointF &point, QModelIndex &index) {
        index = m_list->indexAt(point.toPoint());
        return index.isValid() ? m_delegate->buttonAt(m_list->visualRect(index), index, point) : StoreCardDelegate::Button::None;
    };
    switch (event->type()) {
    case QEvent::MouseMove: {
        QModelIndex index;
        const auto button = locate(static_cast<QMouseEvent *>(event)->position(), index);
        m_delegate->setHover(index.row(), button);
        m_list->viewport()->setCursor(button != StoreCardDelegate::Button::None ? Qt::PointingHandCursor : Qt::ArrowCursor);
        m_list->viewport()->update();
        break;
    }
    case QEvent::Leave:
        m_delegate->setHover(-1, StoreCardDelegate::Button::None);
        m_list->viewport()->update();
        break;
    case QEvent::MouseButtonPress: {
        QModelIndex index;
        const auto button = locate(static_cast<QMouseEvent *>(event)->position(), index);
        m_delegate->setPressed(index.row(), button);
        m_list->viewport()->update();
        break;
    }
    case QEvent::MouseButtonRelease: {
        QModelIndex index;
        const auto button = locate(static_cast<QMouseEvent *>(event)->position(), index);
        m_delegate->setPressed(-1, StoreCardDelegate::Button::None);
        m_list->viewport()->update();
        if (!index.isValid()) {
            break;
        }
        const std::string id = fromQString(index.data(StoreCardDelegate::IdRole).toString());
        if (button == StoreCardDelegate::Button::Play && index.data(StoreCardDelegate::CanPlayRole).toBool()) {
            play(id);
            return true;
        }
        if (button == StoreCardDelegate::Button::Save && index.data(StoreCardDelegate::CanSaveRole).toBool()) {
            save(id);
            return true;
        }
        break;
    }
    default:
        break;
    }
    return false;
}

}  // namespace sky
