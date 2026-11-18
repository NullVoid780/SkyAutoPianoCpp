#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>

#include <QObject>

#include "core/result.hpp"
#include "model/store_item.hpp"

namespace sky {

class HttpClient;
struct HttpRequest;

inline constexpr std::int64_t kStoreListCacheMs = 30000;
inline constexpr std::int64_t kStorePlayerCacheMs = 120000;
inline constexpr qint64 kStoreListMaxBytes = 256 * 1024;
inline constexpr qint64 kStorePlayerMaxBytes = 600 * 1024;
inline constexpr qint64 kStoreDownloadMaxBytes = 2 * 1024 * 1024;

struct StoreQuery {
    std::string q;
    std::int64_t limit = 20;
    std::int64_t offset = 0;
    bool refresh = false;
};

class StoreService final : public QObject {
    Q_OBJECT

public:
    using PageCallback = std::function<void(Result<StorePage>)>;
    using PlayerCallback = std::function<void(Result<StorePlayerPayload>)>;

    explicit StoreService(QObject *parent = nullptr);

    void listSheets(const StoreQuery &query, PageCallback done);
    void getPlayerSheet(const std::string &id, const std::string &sourceType, PlayerCallback done);
    void recordPlayerStart(const std::string &id, const std::string &sourceType);
    void recordDownload(const std::string &id, const std::string &sourceType);

    const std::string &anonymousUserId() const { return m_anonymousUserId; }

private:
    template <typename T>
    struct CacheEntry {
        std::int64_t timestamp = 0;
        T data;
    };

    HttpRequest baseRequest(const QString &path) const;

    HttpClient *m_http;
    std::string m_anonymousUserId;
    std::map<std::string, CacheEntry<StorePage>> m_listCache;
    std::map<std::string, CacheEntry<StorePlayerPayload>> m_playerCache;
};

}  // namespace sky
