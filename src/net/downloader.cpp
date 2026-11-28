#include "net/downloader.hpp"

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <memory>

#include "core/translator.hpp"
#include "core/logger.hpp"
#include "core/string_utils.hpp"
#include "net/url.hpp"

namespace sky {

namespace {

constexpr int kTransferTimeoutMs = 60000;

Error downloadError(const QString &details)
{
    return Error{"DOWNLOAD_FAILED", trUtf8("Unable to complete update."), fromQString(details)};
}

}  // namespace

Downloader::Downloader(std::vector<QString> allowedHosts, QObject *parent)
    : QObject(parent), m_allowedHosts(std::move(allowedHosts)), m_manager(new QNetworkAccessManager(this))
{
}

void Downloader::cancel()
{
    if (m_reply) {
        m_reply->abort();
    }
}

void Downloader::download(const QUrl &url, const std::filesystem::path &file, qint64 maxBytes, Progress progress,
                          Done done)
{
    if (m_reply) {
        done(downloadError(QStringLiteral("a download is already running")));
        return;
    }
    if (!isAllowedHttpsUrl(url, m_allowedHosts)) {
        done(downloadError(QStringLiteral("host not allowed: ") + url.host()));
        return;
    }
    auto output = std::make_shared<QFile>(QString::fromStdWString(file.wstring()));
    if (!output->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        done(downloadError(output->errorString()));
        return;
    }
    QNetworkRequest request(url);
    request.setTransferTimeout(kTransferTimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);
    request.setRawHeader("User-Agent", "SkyAutoPiano-Updater");
    Logger::info("http", "GET " + fromQString(url.host() + url.path()));
    m_reply = m_manager->get(request);
    QNetworkReply *reply = m_reply;
    auto failure = std::make_shared<QString>();

    connect(reply, &QNetworkReply::redirected, reply, [this, reply, failure](const QUrl &target) {
        if (isAllowedHttpsUrl(target, m_allowedHosts)) {
            emit reply->redirectAllowed();
        } else {
            *failure = QStringLiteral("redirect to a disallowed host: ") + target.host();
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::readyRead, reply, [reply, output, maxBytes, failure] {
        output->write(reply->readAll());
        if (maxBytes > 0 && output->size() > maxBytes) {
            *failure = QStringLiteral("download exceeds the size limit");
            reply->abort();
        }
    });
    auto lastPercent = std::make_shared<int>(-1);
    connect(reply, &QNetworkReply::downloadProgress, reply, [progress, lastPercent](qint64 received, qint64 total) {
        if (!progress || total <= 0) {
            return;
        }
        const int percent = static_cast<int>(received * 100 / total);
        if (percent != *lastPercent) {
            *lastPercent = percent;
            progress(percent);
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, output, failure, done = std::move(done)] {
        m_reply = nullptr;
        reply->deleteLater();
        output->write(reply->readAll());
        output->close();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (!failure->isEmpty()) {
            output->remove();
            done(downloadError(*failure));
            return;
        }
        if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
            output->remove();
            done(downloadError(reply->errorString()));
            return;
        }
        done({});
    });
}

}  // namespace sky
