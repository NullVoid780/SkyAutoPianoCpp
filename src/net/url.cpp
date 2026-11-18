#include "net/url.hpp"

#include <QByteArray>

#include <algorithm>

#include "core/string_utils.hpp"

namespace sky {

QUrl buildUrl(const QString &base, const QString &path, const QueryParameters &query)
{
    QUrl url(base + path);
    if (!query.empty()) {
        QByteArray encoded;
        for (const auto &[key, value] : query) {
            if (!encoded.isEmpty()) {
                encoded += '&';
            }
            encoded += QUrl::toPercentEncoding(key) + '=' + QUrl::toPercentEncoding(value);
        }
        url.setQuery(QString::fromLatin1(encoded), QUrl::StrictMode);
    }
    return url;
}

bool isAllowedHttpsUrl(const QUrl &url, const std::vector<QString> &allowedHosts)
{
    if (!url.isValid() || url.scheme() != QStringLiteral("https")) {
        return false;
    }
    const QString host = url.host().toLower();
    return std::find(allowedHosts.begin(), allowedHosts.end(), host) != allowedHosts.end();
}

std::string normalizeAllowedUrl(std::string_view raw, const QString &base, const QString &allowedHost)
{
    const std::string trimmed(trimJs(raw));
    if (trimmed.empty()) {
        return {};
    }
    if (trimmed.front() == '/') {
        return fromQString(base) + trimmed;
    }
    QUrl url(toQString(trimmed), QUrl::StrictMode);
    if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host() != allowedHost) {
        return {};
    }
    if (url.path().isEmpty()) {
        url.setPath(QStringLiteral("/"));
    }
    return fromQString(url.toString(QUrl::FullyEncoded));
}

}  // namespace sky
