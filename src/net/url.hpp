#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QString>
#include <QUrl>

namespace sky {

using QueryParameters = std::vector<std::pair<QString, QString>>;

QUrl buildUrl(const QString &base, const QString &path, const QueryParameters &query = {});
bool isAllowedHttpsUrl(const QUrl &url, const std::vector<QString> &allowedHosts);
std::string normalizeAllowedUrl(std::string_view raw, const QString &base, const QString &allowedHost);

}  // namespace sky
