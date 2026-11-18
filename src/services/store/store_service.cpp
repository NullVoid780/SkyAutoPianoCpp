#include "services/store/store_service.hpp"

#include <algorithm>

#include "core/json.hpp"
#include "core/logger.hpp"
#include "core/random.hpp"
#include "core/string_utils.hpp"
#include "core/time_utils.hpp"
#include "net/http_client.hpp"
#include "net/url.hpp"
#include "services/store/store_mapping.hpp"

namespace sky {

namespace {

constexpr const char *kUserAgent = "SkyAutoPiano/1.3.7a";
constexpr const char *kRequestFailed = "Sky Sheet Store request failed.";

Result<OrderedJson> parseStoreResponse(const Result<HttpResponse> &response, const char *networkFallback)
{
    if (!response) {
        return Error{response.error().code, networkFallback, response.error().details};
    }
    const HttpResponse &http = response.value();
    auto body = parseJson<OrderedJson>(std::string_view(http.body.constData(), static_cast<std::size_t>(http.body.size())));
    if (http.isSuccess()) {
        if (!body) {
            return Error{"STORE_INVALID_RESPONSE", networkFallback, body.error().userMessage};
        }
        return std::move(body).value();
    }
    std::string message;
    std::string code;
    if (body && body.value().is_object()) {
        const OrderedJson &json = body.value();
        if (json.contains("message") && json["message"].is_string()) {
            message = json["message"].get<std::string>();
        }
        if (json.contains("error") && json["error"].is_string()) {
            code = json["error"].get<std::string>();
        }
    }
    return Error{code.empty() ? "http_" + std::to_string(http.status) : code,
                 message.empty() ? std::string(kRequestFailed) : message, std::to_string(http.status)};
}

std::string normalizedSourceType(const std::string &sourceType)
{
    return sourceType == "archive" ? "archive" : "user";
}

}  // namespace

StoreService::StoreService(QObject *parent)
    : QObject(parent), m_http(new HttpClient({QString::fromLatin1(kStoreApiHost)}, this)), m_anonymousUserId(uuidV4())
{
}

HttpRequest StoreService::baseRequest(const QString &path) const
{
    HttpRequest request;
    request.url = buildUrl(QString::fromLatin1(kStoreApiBase), path);
    request.headers = {{"Accept", "application/json"}, {"User-Agent", kUserAgent}};
    return request;
}

void StoreService::listSheets(const StoreQuery &query, PageCallback done)
{
    const std::string q(trimJs(query.q));
    const std::int64_t limit = clampInteger(OrderedJson(query.limit), 1, 100, 20);
    const std::int64_t offset = clampInteger(OrderedJson(query.offset), 0, kMaxSafeInteger, 0);
    OrderedJson keyJson = OrderedJson::object();
    keyJson["q"] = q;
    keyJson["limit"] = limit;
    keyJson["offset"] = offset;
    const std::string cacheKey = dumpJson(keyJson);

    if (!query.refresh) {
        const auto cached = m_listCache.find(cacheKey);
        if (cached != m_listCache.end() && steadyMillis() - cached->second.timestamp < kStoreListCacheMs) {
            done(cached->second.data);
            return;
        }
    }

    QueryParameters params;
    if (!q.empty()) {
        params.emplace_back(QStringLiteral("q"), toQString(q));
    }
    params.emplace_back(QStringLiteral("limit"), QString::number(limit));
    params.emplace_back(QStringLiteral("offset"), QString::number(offset));
    params.emplace_back(QStringLiteral("sort"), QStringLiteral("newest"));

    HttpRequest request = baseRequest(QStringLiteral("/api/v1/catalog/sheets"));
    request.url = buildUrl(QString::fromLatin1(kStoreApiBase), QStringLiteral("/api/v1/catalog/sheets"), params);
    request.maxBytes = kStoreListMaxBytes;
    m_http->send(request, [this, cacheKey, limit, offset, done = std::move(done)](Result<HttpResponse> response) {
        auto json = parseStoreResponse(response, "Unable to connect to Sky Sheet Store.");
        if (!json) {
            done(json.error());
            return;
        }
        auto page = mapStorePage(json.value(), limit, offset);
        if (page) {
            m_listCache[cacheKey] = CacheEntry<StorePage>{steadyMillis(), page.value()};
        }
        done(std::move(page));
    });
}

void StoreService::getPlayerSheet(const std::string &rawId, const std::string &sourceType, PlayerCallback done)
{
    const std::string id(trimJs(rawId));
    if (!isStoreSheetId(id)) {
        done(Error{"STORE_INVALID_ID", "Invalid Sky Sheet Store sheet id.", {}});
        return;
    }
    const std::string type = normalizedSourceType(sourceType);
    const std::string cacheKey = type + ":" + id;
    const auto cached = m_playerCache.find(cacheKey);
    if (cached != m_playerCache.end() && steadyMillis() - cached->second.timestamp < kStorePlayerCacheMs) {
        done(cached->second.data);
        return;
    }
    const QString path = type == "archive" ? QStringLiteral("/api/v1/archive/sheets/%1/player").arg(toQString(id))
                                           : QStringLiteral("/api/v1/sheets/%1/player").arg(toQString(id));
    HttpRequest request = baseRequest(path);
    request.maxBytes = kStorePlayerMaxBytes;
    m_http->send(request, [this, cacheKey, done = std::move(done)](Result<HttpResponse> response) {
        auto json = parseStoreResponse(response, "Unable to load this sheet. Please try again.");
        if (!json) {
            done(json.error());
            return;
        }
        if (!json.value().is_object() && !json.value().is_array()) {
            done(Error{"STORE_INVALID_RESPONSE", "Sky Sheet Store returned an invalid player payload.", {}});
            return;
        }
        auto payload = mapStorePlayerPayload(json.value());
        if (payload) {
            m_playerCache[cacheKey] = CacheEntry<StorePlayerPayload>{steadyMillis(), payload.value()};
        }
        done(std::move(payload));
    });
}

void StoreService::recordPlayerStart(const std::string &rawId, const std::string &sourceType)
{
    const std::string id(trimJs(rawId));
    if (!isStoreSheetId(id) || normalizedSourceType(sourceType) == "archive") {
        return;
    }
    HttpRequest request = baseRequest(QStringLiteral("/api/v1/sheets/%1/open").arg(toQString(id)));
    request.method = "POST";
    request.maxBytes = kStorePlayerMaxBytes;
    request.headers.emplace_back("X-Anonymous-User-Id", QByteArray::fromStdString(m_anonymousUserId));
    m_http->send(request, [](Result<HttpResponse> response) {
        if (!response || !response.value().isSuccess()) {
            Logger::warning("store", "Failed to record Sky Sheet Store player start");
        }
    });
}

void StoreService::recordDownload(const std::string &rawId, const std::string &sourceType)
{
    const std::string id(trimJs(rawId));
    if (!isStoreSheetId(id)) {
        return;
    }
    const QString path = normalizedSourceType(sourceType) == "archive"
                             ? QStringLiteral("/api/v1/archive/sheets/%1/download").arg(toQString(id))
                             : QStringLiteral("/api/v1/sheets/%1/download").arg(toQString(id));
    HttpRequest request = baseRequest(path);
    request.maxBytes = kStoreDownloadMaxBytes;
    request.headers = {{"Accept", "application/json,text/plain,*/*"},
                       {"User-Agent", kUserAgent},
                       {"X-Anonymous-User-Id", QByteArray::fromStdString(m_anonymousUserId)}};
    m_http->send(request, [](Result<HttpResponse> response) {
        if (!response || !response.value().isSuccess()) {
            Logger::warning("store", "Failed to record Sky Sheet Store download");
        }
    });
}

}  // namespace sky
