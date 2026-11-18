#include "net/http_client.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <memory>

#include "core/logger.hpp"
#include "core/string_utils.hpp"
#include "net/url.hpp"

namespace sky {

HttpClient::HttpClient(std::vector<QString> allowedHosts, QObject *parent)
    : QObject(parent), m_allowedHosts(std::move(allowedHosts)), m_manager(new QNetworkAccessManager(this))
{
}

void HttpClient::send(const HttpRequest &request, Callback done)
{
    if (!isAllowedHttpsUrl(request.url, m_allowedHosts)) {
        done(Error{"URL_REJECTED", "The request address is not allowed.", fromQString(request.url.toString())});
        return;
    }

    QNetworkRequest networkRequest(request.url);
    networkRequest.setTransferTimeout(request.timeoutMs);
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);
    for (const auto &[name, value] : request.headers) {
        networkRequest.setRawHeader(name, value);
    }
    Logger::info("http", request.method.toStdString() + " " + fromQString(request.url.host() + request.url.path()));

    QNetworkReply *reply = m_manager->sendCustomRequest(networkRequest, request.method, request.body);
    auto tooLarge = std::make_shared<bool>(false);
    connect(reply, &QNetworkReply::redirected, reply, [this, reply](const QUrl &target) {
        if (isAllowedHttpsUrl(target, m_allowedHosts)) {
            emit reply->redirectAllowed();
        } else {
            Logger::warning("http", "blocked redirect to " + fromQString(target.host()));
            reply->abort();
        }
    });
    if (request.maxBytes > 0) {
        const qint64 cap = request.maxBytes;
        connect(reply, &QNetworkReply::metaDataChanged, reply, [reply, cap, tooLarge] {
            const QVariant length = reply->header(QNetworkRequest::ContentLengthHeader);
            if (length.isValid() && length.toLongLong() > cap) {
                *tooLarge = true;
                reply->abort();
            }
        });
        connect(reply, &QNetworkReply::downloadProgress, reply, [reply, cap, tooLarge](qint64 received, qint64) {
            if (received > cap) {
                *tooLarge = true;
                reply->abort();
            }
        });
    }
    connect(reply, &QNetworkReply::finished, this, [reply, tooLarge, done = std::move(done)] {
        reply->deleteLater();
        if (*tooLarge) {
            done(Error{"RESPONSE_TOO_LARGE", "The response was too large.", {}});
            return;
        }
        const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (!status.isValid() || status.toInt() == 0) {
            done(Error{"NETWORK_ERROR", fromQString(reply->errorString()), fromQString(reply->errorString())});
            return;
        }
        HttpResponse response{status.toInt(), reply->readAll(), reply->url(), {}};
        for (const auto &[name, value] : reply->rawHeaderPairs()) {
            response.headers.emplace_back(name, value);
        }
        done(std::move(response));
    });
}

}  // namespace sky
