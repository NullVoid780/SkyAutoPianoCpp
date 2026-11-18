#include "ui/main_window/library_model.hpp"

#include <algorithm>

#include "core/json.hpp"
#include "core/string_utils.hpp"
#include "services/library/favorites_service.hpp"
#include "services/library/library_service.hpp"
#include "services/library/recent_service.hpp"

namespace sky {

namespace {

QString bpmText(const SheetEntry &entry)
{
    const OrderedJson *bpm = entry.field("bpm");
    if (!bpm || !jsTruthy(*bpm)) {
        return {};
    }
    if (bpm->is_string()) {
        return toQString(bpm->get<std::string>());
    }
    if (bpm->is_number()) {
        return toQString(formatJsNumber(bpm->get<double>()));
    }
    return bpm->is_boolean() ? QStringLiteral("true") : QString();
}

QString orUnknown(const std::string &value)
{
    return value.empty() ? QStringLiteral("Unknown") : toQString(value);
}

}  // namespace

LibraryModel::LibraryModel(LibraryService &library, FavoritesService &favorites, RecentService &recent, QObject *parent)
    : QAbstractListModel(parent), m_library(library), m_favorites(favorites), m_recent(recent)
{
    connect(&m_library, &LibraryService::changed, this, &LibraryModel::refresh);
    connect(&m_favorites, &FavoritesService::changed, this, &LibraryModel::refresh);
    connect(&m_recent, &RecentService::changed, this, &LibraryModel::refresh);
    refresh();
}

QString LibraryModel::tabTitle(LibraryTab tab)
{
    switch (tab) {
    case LibraryTab::Favorite:
        return QStringLiteral("Favorite");
    case LibraryTab::RecentPlay:
        return QStringLiteral("Recent Play");
    default:
        return QStringLiteral("All Songs");
    }
}

bool LibraryModel::matches(int libraryIndex) const
{
    const SheetEntry *entry = m_library.entry(libraryIndex);
    if (!entry) {
        return false;
    }
    const std::string name = entry->name();
    if (m_tab == LibraryTab::Favorite && !m_favorites.isFavorite(name)) {
        return false;
    }
    if (m_tab == LibraryTab::RecentPlay && !m_recent.contains(name)) {
        return false;
    }
    if (m_search.isEmpty()) {
        return true;
    }
    return toQString(name).toLower().contains(m_search) || toQString(entry->author()).toLower().contains(m_search);
}

void LibraryModel::setBulkMode(bool enabled)
{
    m_bulkMode = enabled;
    if (!enabled) {
        m_selected.clear();
    }
    if (!m_rows.empty()) {
        emit dataChanged(index(0), index(rowCount() - 1));
    }
    emit selectionChanged();
}

void LibraryModel::toggleSelected(const std::string &payloadKey)
{
    if (payloadKey.empty()) {
        return;
    }
    if (!m_selected.erase(payloadKey)) {
        m_selected.insert(payloadKey);
    }
    if (!m_rows.empty()) {
        emit dataChanged(index(0), index(rowCount() - 1));
    }
    emit selectionChanged();
}

void LibraryModel::setSelected(const std::set<std::string> &keys)
{
    m_selected = keys;
    if (!m_rows.empty()) {
        emit dataChanged(index(0), index(rowCount() - 1));
    }
    emit selectionChanged();
}

std::vector<std::string> LibraryModel::visibleKeys() const
{
    std::vector<std::string> keys;
    for (int index : m_rows) {
        if (const SheetEntry *entry = m_library.entry(index)) {
            keys.push_back(entry->payloadFile());
        }
    }
    return keys;
}

void LibraryModel::refresh()
{
    std::set<std::string> available;
    for (const SheetEntry &entry : m_library.entries()) {
        available.insert(entry.payloadFile());
    }
    std::erase_if(m_selected, [&available](const std::string &key) { return !available.contains(key); });

    beginResetModel();
    m_rows.clear();
    if (m_restricted) {
        for (int index : m_restriction) {
            if (matches(index)) {
                m_rows.push_back(index);
            }
        }
    } else {
        for (int index = 0; index < m_library.size(); ++index) {
            if (matches(index)) {
                m_rows.push_back(index);
            }
        }
    }
    endResetModel();
    emit selectionChanged();
}

void LibraryModel::setTab(LibraryTab tab)
{
    m_tab = tab;
    refresh();
}

void LibraryModel::setSearchTerm(const QString &term)
{
    m_search = term.toLower().trimmed();
    refresh();
}

void LibraryModel::setRestriction(std::vector<int> indices, bool restricted)
{
    m_restriction = std::move(indices);
    m_restricted = restricted;
    refresh();
}

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int LibraryModel::libraryIndex(int row) const
{
    return row >= 0 && row < static_cast<int>(m_rows.size()) ? m_rows[static_cast<std::size_t>(row)] : -1;
}

int LibraryModel::rowForLibraryIndex(int libraryIndex) const
{
    const auto it = std::find(m_rows.begin(), m_rows.end(), libraryIndex);
    return it == m_rows.end() ? -1 : static_cast<int>(it - m_rows.begin());
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    const SheetEntry *entry = m_library.entry(libraryIndex(index.row()));
    if (!entry) {
        return {};
    }
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return toQString(entry->name());
    case AuthorRole:
        return orUnknown(entry->author());
    case TranscriberRole:
        return orUnknown(entry->transcribedBy());
    case BpmRole:
        return bpmText(*entry);
    case HoldRole:
        return entry->hasHoldNotes();
    case LibraryIndexRole:
        return libraryIndex(index.row());
    case FavoriteRole:
        return m_favorites.isFavorite(entry->name());
    case PayloadKeyRole:
        return toQString(entry->payloadFile());
    case SelectedRole:
        return m_selected.contains(entry->payloadFile());
    case BulkModeRole:
        return m_bulkMode;
    default:
        return {};
    }
}

}  // namespace sky
