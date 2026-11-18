#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <QObject>

#include "core/result.hpp"
#include "model/playlist.hpp"

namespace sky {

class PlaylistService final : public QObject {
    Q_OBJECT

public:
    explicit PlaylistService(std::filesystem::path dataDirectory, QObject *parent = nullptr);

    void load();
    const std::vector<Playlist> &playlists() const { return m_playlists; }
    const Playlist *find(std::string_view id) const;

    Result<Playlist> create(std::string_view rawName);
    Result<void> rename(std::string_view id, std::string_view rawName);
    bool remove(std::string_view id);
    Result<void> addSheet(std::string_view id, const std::string &sheetKey);
    Result<int> addSheets(std::string_view id, const std::vector<std::string> &sheetKeys);
    void removeSheet(std::string_view id, const std::string &sheetKey);
    bool moveSheet(std::string_view id, const std::string &sheetKey, int direction);
    void removeSheetsEverywhere(const std::set<std::string> &sheetKeys);

    static std::string normalizeName(std::string_view rawName);
    static std::string bulkAddMessage(int added, const std::string &playlistName);

signals:
    void changed();

private:
    Playlist *findMutable(std::string_view id);
    bool nameTaken(const std::string &name, std::string_view exceptId) const;
    void save();

    std::filesystem::path m_path;
    std::vector<Playlist> m_playlists;
};

}  // namespace sky
