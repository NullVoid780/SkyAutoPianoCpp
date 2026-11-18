#pragma once

#include <filesystem>
#include <functional>
#include <vector>

#include <QObject>
#include <QString>
#include <QUrl>

#include "core/result.hpp"

class QNetworkAccessManager;
class QNetworkReply;

namespace sky {

class Downloader final : public QObject {
    Q_OBJECT

public:
    using Progress = std::function<void(int percent)>;
    using Done = std::function<void(Result<void>)>;

    Downloader(std::vector<QString> allowedHosts, QObject *parent = nullptr);

    void download(const QUrl &url, const std::filesystem::path &file, qint64 maxBytes, Progress progress, Done done);
    void cancel();
    bool isRunning() const { return m_reply != nullptr; }

private:
    std::vector<QString> m_allowedHosts;
    QNetworkAccessManager *m_manager;
    QNetworkReply *m_reply = nullptr;
};

}  // namespace sky
