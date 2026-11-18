#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QDateTime>
#include <QObject>
#include <QString>

#include "core/json.hpp"
#include "core/result.hpp"
#include "services/update/release_notes.hpp"
#include "services/update/github_release.hpp"
#include "services/update/version.hpp"

namespace sky {

class Downloader;
class HttpClient;

struct VersionCheck {
    std::string currentVersion;
    std::string latestVersion;
    bool available = false;
    bool critical = false;
    std::string notes;
    std::string releasePage;
    std::optional<UpdatePackage> package;
    std::vector<VersionNotes> history;
};

struct UpdateInfo {
    std::string previousVersion;
    std::string newVersion;
    std::string updateTime;
    bool showChangelog = false;
    std::string notes;
};

class UpdateService final : public QObject {
    Q_OBJECT

public:
    UpdateService(std::string currentVersion, std::filesystem::path appDirectory, std::filesystem::path configDirectory,
                  QObject *parent = nullptr);

    void checkForUpdates(std::function<void(Result<VersionCheck>)> done);
    void fetchChangelog(std::function<void(Result<std::vector<VersionNotes>>)> done);
    void performUpdate(const VersionCheck &check);
    void cleanupPendingFiles();
    bool launchPendingInstaller();

    bool isUpdating() const { return m_updating; }
    bool isReady() const { return m_ready; }
    int progress() const { return m_progress; }
    const std::string &currentVersion() const { return m_currentVersion; }
    const std::optional<VersionCheck> &lastCheck() const { return m_lastCheck; }

    std::optional<UpdateInfo> pendingChangelog() const;
    void markChangelogShown();

    static Result<VersionCheck> evaluateReleases(const Json &releases, std::string_view currentVersion,
                                                 InstallKind kind = InstallKind::Installed);
    static InstallKind detectInstallKind(const std::filesystem::path &appDirectory);
    InstallKind installKind() const { return m_installKind; }
    static VersionCheck evaluateLatestTag(std::string_view latestVersion, std::string_view currentVersion,
                                          std::vector<VersionNotes> changelog,
                                          InstallKind kind = InstallKind::Installed);
    static Result<std::vector<std::filesystem::path>> applyStagedFiles(const std::filesystem::path &staging,
                                                                       const std::filesystem::path &appDirectory,
                                                                       const std::filesystem::path &cleanupList);

signals:
    void progressChanged(int percent);
    void updateReady();
    void updateFailed(const QString &message);

private:
    Result<VersionCheck> evaluateBody(const QByteArray &body) const;
    void checkWithoutApi(std::function<void(Result<VersionCheck>)> done);
    bool apiPaused() const;
    void loadReleaseCache();
    void storeReleaseCache(const QByteArray &etag, const QByteArray &body);
    void fail(const std::string &details);
    void finishInstall(const std::filesystem::path &file, bool installer);
    void writeUpdateInfo(const UpdateInfo &info) const;

    std::string m_currentVersion;
    std::filesystem::path m_appDirectory;
    InstallKind m_installKind = InstallKind::Installed;
    std::filesystem::path m_configDirectory;
    HttpClient *m_http;
    Downloader *m_downloader;
    bool m_updating = false;
    bool m_ready = false;
    int m_progress = 0;
    std::filesystem::path m_pendingInstaller;
    std::optional<VersionCheck> m_lastCheck;
    QDateTime m_apiBlockedUntil;
    QByteArray m_releaseEtag;
    QByteArray m_releaseBody;
    QDateTime m_lastApiAnswer;
};

}  // namespace sky
