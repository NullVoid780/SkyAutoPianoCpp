#pragma once

#include <functional>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>

#include "core/result.hpp"

class QNetworkAccessManager;

namespace sky {

inline constexpr int kDefaultHttpTimeoutMs = 15000;

struct HttpRequest {
    QUrl url;
    QByteArray method = "GET";
    std::vector<std::pair<QByteArray, QByteArray>> headers;
    QByteArray body;
    qint64 maxBytes = 0;
    int timeoutMs = kDefaultHttpTimeoutMs;
};

struct HttpResponse {
    int status = 0;
    QByteArray body;
    QUrl url;
    std::vector<std::pair<QByteArray, QByteArray>> headers;

    bool isSuccess() const { return status >= 200 && status < 300; }

    QByteArray header(const QByteArray &name) const
    {
        const QByteArray wanted = name.toLower();
        for (const auto &[key, value] : headers) {
            if (key.toLower() == wanted) {
                return value;
            }
        }
        return {};
    }
};

class HttpClient final : public QObject {
    Q_OBJECT

public:
    using Callback = std::function<void(Result<HttpResponse>)>;

    explicit HttpClient(std::vector<QString> allowedHosts, QObject *parent = nullptr);

    void send(const HttpRequest &request, Callback done);

private:
    std::vector<QString> m_allowedHosts;
    QNetworkAccessManager *m_manager;
};

}  // namespace sky
