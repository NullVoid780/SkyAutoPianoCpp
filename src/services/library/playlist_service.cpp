#include "services/library/playlist_service.hpp"

#include <algorithm>

#include "core/json.hpp"
#include "core/logger.hpp"
#include "core/random.hpp"
#include "core/string_utils.hpp"
#include "core/time_utils.hpp"
#include "core/translator.hpp"

namespace sky {

namespace {

std::string truncateUtf16(std::string_view text, std::size_t maxUnits) {
    std::size_t units = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t next = pos;
        const std::size_t width = decodeUtf8At(text, next) >= 0x10000 ? 2 : 1;
        if (units + width > maxUnits) {
            break;
        }
        units += width;
        pos = next;
    }
    return std::string(text.substr(0, pos));
}

bool sameNameIgnoringCase(const std::string &a, const std::string &b) {
    return toQString(a).toLower() == toQString(b).toLower();
}

Error warning(const char *code, const char *message) {
    return Error{code, trUtf8(message), {}};
}

}  // namespace

PlaylistService::PlaylistService(std::filesystem::path dataDirectory, QObject *parent)
    : PlaylistService(std::move(dataDirectory), StorageFileOps::native(), parent) {}

PlaylistService::PlaylistService(std::filesystem::path dataDirectory, StorageFileOps fileOps,
                                 QObject *parent)
    : QObject(parent), m_path(std::move(dataDirectory) / "playlists.json"),
      m_fileOps(StorageFileOps::withDefaults(std::move(fileOps))) {}

std::string PlaylistService::normalizeName(std::string_view rawName) {
    return truncateUtf16(trimJs(rawName), kPlaylistNameMaxLength);
}

std::string PlaylistService::bulkAddMessage(int added, const std::string &playlistName) {
    const QString name = toQString(playlistName);
    if (added == 1) {
        return fromQString(tr("Added 1 sheet to %1.").arg(name));
    }
    if (added > 1) {
        return fromQString(tr("Added %1 sheets to %2.").arg(QString::number(added), name));
    }
    return fromQString(tr("All selected sheets are already in %1.").arg(name));
}

void PlaylistService::load() {
    m_playlists.clear();
    m_lastError.reset();
    if (!m_fileOps.exists(m_path)) {
        emit changed();
        return;
    }
    auto text = m_fileOps.readText(m_path);
    auto parsed = text ? parseJson<OrderedJson>(text.value()) : Result<OrderedJson>(text.error());
    if (!parsed) {
        Logger::error("playlists", "Failed to load playlists: " + parsed.error().userMessage);
        emit changed();
        return;
    }
    if (!parsed.value().is_array()) {
        emit changed();
        return;
    }
    for (const OrderedJson &item : parsed.value()) {
        Playlist playlist;
        if (item.is_object()) {
            if (item.contains("id") && item["id"].is_string()) {
                playlist.id = item["id"].get<std::string>();
            }
            if (item.contains("name") && item["name"].is_string()) {
                playlist.name = normalizeName(item["name"].get<std::string>());
            }
            if (item.contains("sheetKeys") && item["sheetKeys"].is_array()) {
                for (const OrderedJson &key : item["sheetKeys"]) {
                    if (!key.is_string() || key.get_ref<const std::string &>().empty()) {
                        continue;
                    }
                    const std::string value = key.get<std::string>();
                    if (std::find(playlist.sheetKeys.begin(), playlist.sheetKeys.end(), value) ==
                        playlist.sheetKeys.end()) {
                        playlist.sheetKeys.push_back(value);
                    }
                }
            }
        }
        if (!playlist.id.empty() && !playlist.name.empty()) {
            m_playlists.push_back(std::move(playlist));
        }
    }
    emit changed();
}

Result<void> PlaylistService::save() {
    OrderedJson array = OrderedJson::array();
    for (const Playlist &playlist : m_playlists) {
        OrderedJson item = OrderedJson::object();
        item["id"] = playlist.id;
        item["name"] = playlist.name;
        item["sheetKeys"] = OrderedJson::array();
        for (const std::string &key : playlist.sheetKeys) {
            item["sheetKeys"].push_back(key);
        }
        array.push_back(std::move(item));
    }
    if (auto ensured = m_fileOps.ensureDir(m_path.parent_path()); !ensured) {
        Logger::error("playlists", ensured.error().details);
        m_lastError = ensured.error();
        return ensured.error();
    }
    auto written = m_fileOps.atomicWrite(m_path, dumpJson(array, 4));
    if (!written) {
        Logger::error("playlists", written.error().details);
        m_lastError = written.error();
        return written.error();
    }
    m_lastError.reset();
    emit changed();
    return {};
}

const Playlist *PlaylistService::find(std::string_view id) const {
    for (const Playlist &playlist : m_playlists) {
        if (playlist.id == id) {
            return &playlist;
        }
    }
    return nullptr;
}

Playlist *PlaylistService::findMutable(std::string_view id) {
    return const_cast<Playlist *>(find(id));
}

bool PlaylistService::nameTaken(const std::string &name, std::string_view exceptId) const {
    return std::any_of(m_playlists.begin(), m_playlists.end(), [&](const Playlist &playlist) {
        return playlist.id != exceptId && sameNameIgnoringCase(playlist.name, name);
    });
}

Result<Playlist> PlaylistService::create(std::string_view rawName) {
    const std::string name = normalizeName(rawName);
    if (name.empty()) {
        return warning("PLAYLIST_NAME_EMPTY", trNoop("Enter a playlist name."));
    }
    if (nameTaken(name, {})) {
        return warning("PLAYLIST_NAME_DUPLICATE",
                       trNoop("A playlist with this name already exists."));
    }
    const auto previous = m_playlists;
    Playlist playlist{
        "playlist-" + std::to_string(epochMillis()) + "-" + randomBase36(6), name, {}};
    m_playlists.push_back(playlist);
    if (auto saved = save(); !saved) {
        m_playlists = previous;
        return saved.error();
    }
    return playlist;
}

Result<void> PlaylistService::rename(std::string_view id, std::string_view rawName) {
    Playlist *playlist = findMutable(id);
    if (!playlist) {
        return {};
    }
    const std::string name = normalizeName(rawName);
    if (name.empty() || nameTaken(name, id)) {
        return warning("PLAYLIST_NAME_INVALID", trNoop("Enter a unique playlist name."));
    }
    const auto previous = m_playlists;
    playlist->name = name;
    if (auto saved = save(); !saved) {
        m_playlists = previous;
        return saved.error();
    }
    return {};
}

bool PlaylistService::remove(std::string_view id) {
    const auto previous = m_playlists;
    std::erase_if(m_playlists, [id](const Playlist &playlist) { return playlist.id == id; });
    if (m_playlists.size() == previous.size()) {
        return false;
    }
    if (auto saved = save(); !saved) {
        m_playlists = previous;
        return false;
    }
    return true;
}

Result<void> PlaylistService::addSheet(std::string_view id, const std::string &sheetKey) {
    Playlist *playlist = findMutable(id);
    if (!playlist || sheetKey.empty()) {
        return {};
    }
    if (std::find(playlist->sheetKeys.begin(), playlist->sheetKeys.end(), sheetKey) !=
        playlist->sheetKeys.end()) {
        return warning("PLAYLIST_SHEET_EXISTS", trNoop("This sheet is already in the playlist."));
    }
    const auto previous = m_playlists;
    playlist->sheetKeys.push_back(sheetKey);
    if (auto saved = save(); !saved) {
        m_playlists = previous;
        return saved.error();
    }
    return {};
}

Result<int> PlaylistService::addSheets(std::string_view id,
                                       const std::vector<std::string> &sheetKeys) {
    Playlist *playlist = findMutable(id);
    if (!playlist) {
        return warning("PLAYLIST_NOT_SELECTED", trNoop("Create or select a playlist first."));
    }
    const auto previous = m_playlists;
    int added = 0;
    for (const std::string &key : sheetKeys) {
        if (std::find(playlist->sheetKeys.begin(), playlist->sheetKeys.end(), key) !=
            playlist->sheetKeys.end()) {
            continue;
        }
        playlist->sheetKeys.push_back(key);
        ++added;
    }
    if (added > 0) {
        if (auto saved = save(); !saved) {
            m_playlists = previous;
            return saved.error();
        }
    }
    return added;
}

Result<void> PlaylistService::removeSheet(std::string_view id, const std::string &sheetKey) {
    Playlist *playlist = findMutable(id);
    if (!playlist) {
        return {};
    }
    const auto previous = m_playlists;
    if (std::erase(playlist->sheetKeys, sheetKey) == 0) {
        return {};
    }
    if (auto saved = save(); !saved) {
        m_playlists = previous;
        return saved.error();
    }
    return {};
}

bool PlaylistService::moveSheet(std::string_view id, const std::string &sheetKey, int direction) {
    Playlist *playlist = findMutable(id);
    if (!playlist) {
        return false;
    }
    auto &keys = playlist->sheetKeys;
    const auto it = std::find(keys.begin(), keys.end(), sheetKey);
    if (it == keys.end()) {
        return false;
    }
    const auto current = static_cast<long long>(it - keys.begin());
    const long long next = current + direction;
    if (next < 0 || next >= static_cast<long long>(keys.size())) {
        return false;
    }
    const auto previous = m_playlists;
    std::swap(keys[static_cast<std::size_t>(current)], keys[static_cast<std::size_t>(next)]);
    if (auto saved = save(); !saved) {
        m_playlists = previous;
        return false;
    }
    return true;
}

Result<void> PlaylistService::removeSheetsEverywhere(const std::set<std::string> &sheetKeys) {
    if (sheetKeys.empty()) {
        return {};
    }
    const auto previous = m_playlists;
    bool changedAny = false;
    for (Playlist &playlist : m_playlists) {
        const auto before = playlist.sheetKeys.size();
        std::erase_if(playlist.sheetKeys,
                      [&](const std::string &key) { return sheetKeys.contains(key); });
        changedAny = changedAny || playlist.sheetKeys.size() != before;
    }
    if (!changedAny) {
        return {};
    }
    if (auto saved = save(); !saved) {
        m_playlists = previous;
        return saved.error();
    }
    return {};
}

}  // namespace sky
