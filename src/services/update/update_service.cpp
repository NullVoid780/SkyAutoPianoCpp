#include "services/update/update_service.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QMetaObject>
#include <QPointer>
#include <QThreadPool>
#include <QUrl>

#include <windows.h>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "core/file_system.hpp"
#include "core/logger.hpp"
#include "core/string_utils.hpp"
#include "core/text_file.hpp"
#include "net/downloader.hpp"
#include "net/http_client.hpp"
#include "net/zip_extract.hpp"

namespace sky {

namespace {

constexpr const char *kRepository = SKY_UPDATE_REPOSITORY;
constexpr qint64 kMaxMetadataBytes = 4 * 1024 * 1024;
constexpr qint64 kMaxDownloadBytes = 512LL * 1024 * 1024;
constexpr const char *kCheckFailed = "Failed to check for updates. Please check your internet connection.";
constexpr const char *kGithubFailed = "GitHub did not answer the update check. Please try again later.";
constexpr const char *kNotPublished = "No release with an installer was found on GitHub yet.";
constexpr const char *kRateLimited = "GitHub is limiting update checks right now. Please try again in a few minutes.";
constexpr qint64 kApiPauseSeconds = 15 * 60;
constexpr qint64 kReuseAnswerSeconds = 60;
constexpr int kReleasesToRead = 20;
constexpr const char *kReleaseCacheFile = "update-cache.json";
constexpr const char *kUpdateInfoFile = "update-info.json";
constexpr const char *kCleanupFile = "cleanup-files.json";
constexpr std::array<std::string_view, 3> kPreservedFolders = {"data", "config", "logs"};

QString releasesApiUrl()
{
    return QStringLiteral("https://api.github.com/repos/%1/releases?per_page=%2")
        .arg(QString::fromLatin1(kRepository))
        .arg(kReleasesToRead);
}

QString latestReleasePageUrl()
{
    return QStringLiteral("https://github.com/%1/releases/latest").arg(QString::fromLatin1(kRepository));
}

QString changelogUrl()
{
    return QStringLiteral("https://raw.githubusercontent.com/%1/main/CHANGELOG.md").arg(QString::fromLatin1(kRepository));
}

std::string releaseTagUrl(std::string_view version, std::string_view file = {})
{
    std::string url = std::string("https://github.com/") + kRepository + "/releases/";
    if (file.empty()) {
        return url + "tag/v" + std::string(version);
    }
    return url + "download/v" + std::string(version) + "/" + std::string(file);
}

std::vector<QString> allowedHosts()
{
    return {QStringLiteral("api.github.com"), QStringLiteral("github.com"),
            QStringLiteral("objects.githubusercontent.com"), QStringLiteral("release-assets.githubusercontent.com"),
            QStringLiteral("raw.githubusercontent.com")};
}

HttpRequest githubRequest(const QString &url)
{
    HttpRequest request;
    request.url = QUrl(url);
    request.maxBytes = kMaxMetadataBytes;
    request.headers = {{"Accept", "application/vnd.github+json"},
                       {"X-GitHub-Api-Version", "2022-11-28"},
                       {"User-Agent", "SkyAutoPiano-Updater"}};
    return request;
}

HttpRequest plainRequest(const QString &url, const QByteArray &method = "GET")
{
    HttpRequest request;
    request.url = QUrl(url);
    request.method = method;
    request.maxBytes = kMaxMetadataBytes;
    request.headers = {{"User-Agent", "SkyAutoPiano-Updater"}, {"Cache-Control", "no-cache"}};
    return request;
}

Error httpError(const Result<HttpResponse> &response)
{
    if (!response) {
        return Error{"UPDATE_OFFLINE", kCheckFailed, response.error().details};
    }
    const int status = response.value().status;
    const std::string details = "http " + std::to_string(status);
    if (status == 404) {
        return Error{"UPDATE_NOT_PUBLISHED", kNotPublished, details};
    }
    if (status == 403 || status == 429) {
        return Error{"UPDATE_RATE_LIMITED", kRateLimited, details};
    }
    return Error{"UPDATE_CHECK_FAILED", kGithubFailed, details};
}

std::string fileSha256(const std::filesystem::path &file)
{
    QFile input(QString::fromStdWString(file.wstring()));
    if (!input.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&input);
    return hash.result().toHex().toStdString();
}

bool isWindowsProgram(const std::filesystem::path &file)
{
    QFile input(QString::fromStdWString(file.wstring()));
    return input.open(QIODevice::ReadOnly) && input.read(2) == "MZ";
}

bool verifyDownload(const std::filesystem::path &file, const UpdatePackage &package)
{
    if (!package.sha256.empty()) {
        return fileSha256(file) == package.sha256;
    }
    std::error_code error;
    if (package.size > 0 && std::filesystem::file_size(file, error) != package.size) {
        return false;
    }
    return package.kind != PackageKind::Installer || isWindowsProgram(file);
}

std::string fileNameFromUrl(const std::string &url)
{
    const std::size_t slash = url.find_last_of('/');
    std::string name = slash == std::string::npos ? url : url.substr(slash + 1);
    std::erase_if(name, [](char c) { return c == '\\' || c == ':' || c == '?' || c == '*' || c == '"'; });
    return name.empty() ? std::string("update.bin") : name;
}

}  // namespace

UpdateService::UpdateService(std::string currentVersion, std::filesystem::path appDirectory,
                             std::filesystem::path configDirectory, QObject *parent)
    : QObject(parent),
      m_currentVersion(std::move(currentVersion)),
      m_appDirectory(std::move(appDirectory)),
      m_installKind(detectInstallKind(m_appDirectory)),
      m_configDirectory(std::move(configDirectory)),
      m_http(new HttpClient(allowedHosts(), this)),
      m_downloader(new Downloader(allowedHosts(), this))
{
    loadReleaseCache();
}

InstallKind UpdateService::detectInstallKind(const std::filesystem::path &appDirectory)
{
    std::error_code error;
    return std::filesystem::exists(appDirectory / "unins000.exe", error) ? InstallKind::Installed
                                                                         : InstallKind::Portable;
}

Result<VersionCheck> UpdateService::evaluateReleases(const Json &releases, std::string_view currentVersion,
                                                     InstallKind kind)
{
    const std::vector<ReleaseInfo> parsed = parseReleases(releases);
    const auto latest = std::find_if(parsed.begin(), parsed.end(),
                                     [kind](const ReleaseInfo &release) { return release.packageFor(kind).has_value(); });
    if (latest == parsed.end()) {
        return Error{"UPDATE_NOT_PUBLISHED", kNotPublished,
                     kind == InstallKind::Portable ? "no release has a portable zip" : "no release has an installer"};
    }
    VersionCheck check;
    check.currentVersion = std::string(currentVersion);
    check.latestVersion = latest->version;
    check.notes = latest->notes;
    check.critical = latest->critical;
    check.releasePage = latest->page.empty() ? releaseTagUrl(latest->version) : latest->page;
    for (const ReleaseInfo &release : parsed) {
        if (!release.notes.empty()) {
            check.history.push_back(VersionNotes{release.version, release.notes});
        }
    }
    if (compareVersions(latest->version, currentVersion) > 0) {
        check.package = latest->packageFor(kind);
        check.available = true;
    }
    return check;
}

VersionCheck UpdateService::evaluateLatestTag(std::string_view latestVersion, std::string_view currentVersion,
                                              std::vector<VersionNotes> changelog, InstallKind kind)
{
    VersionCheck check;
    check.currentVersion = std::string(currentVersion);
    check.latestVersion = std::string(latestVersion);
    check.releasePage = releaseTagUrl(latestVersion);
    if (const VersionNotes *notes = findVersionNotes(changelog, latestVersion)) {
        check.notes = notes->markdown;
    }
    check.history = std::move(changelog);
    if (compareVersions(latestVersion, currentVersion) > 0) {
        check.package = kind == InstallKind::Portable
                            ? UpdatePackage{releaseTagUrl(latestVersion, portableFileName(latestVersion)),
                                            PackageKind::Zip, 0, {}}
                            : UpdatePackage{releaseTagUrl(latestVersion, installerFileName(latestVersion)),
                                            PackageKind::Installer, 0, {}};
        check.available = true;
    }
    return check;
}

Result<VersionCheck> UpdateService::evaluateBody(const QByteArray &body) const
{
    auto parsed = parseJson<Json>(std::string_view(body.constData(), static_cast<std::size_t>(body.size())));
    if (!parsed || !parsed.value().is_array()) {
        return Error{"UPDATE_CHECK_FAILED", kGithubFailed, "invalid release list"};
    }
    return evaluateReleases(parsed.value(), m_currentVersion, m_installKind);
}

bool UpdateService::apiPaused() const
{
    return m_apiBlockedUntil.isValid() && QDateTime::currentDateTimeUtc() < m_apiBlockedUntil;
}

void UpdateService::loadReleaseCache()
{
    auto text = readTextFile(m_configDirectory / kReleaseCacheFile);
    if (!text) {
        return;
    }
    auto parsed = parseJson<Json>(text.value());
    if (!parsed || !parsed.value().is_object() || parsed.value().value("repository", std::string()) != kRepository) {
        return;
    }
    m_releaseEtag = QByteArray::fromStdString(parsed.value().value("etag", std::string()));
    m_releaseBody = QByteArray::fromStdString(parsed.value().value("body", std::string()));
    if (m_releaseEtag.isEmpty() || m_releaseBody.isEmpty()) {
        m_releaseEtag.clear();
        m_releaseBody.clear();
    }
}

void UpdateService::storeReleaseCache(const QByteArray &etag, const QByteArray &body)
{
    m_lastApiAnswer = QDateTime::currentDateTimeUtc();
    if (etag == m_releaseEtag && body == m_releaseBody) {
        return;
    }
    m_releaseEtag = etag;
    m_releaseBody = body;
    if (etag.isEmpty()) {
        return;
    }
    OrderedJson json = OrderedJson::object();
    json["repository"] = kRepository;
    json["etag"] = etag.toStdString();
    json["body"] = body.toStdString();
    (void)atomicWrite(m_configDirectory / kReleaseCacheFile, dumpJson(json));
}

void UpdateService::checkForUpdates(std::function<void(Result<VersionCheck>)> finished)
{
    QPointer<UpdateService> self(this);
    const auto done = [self, finished](Result<VersionCheck> check) {
        if (self && check) {
            self->m_lastCheck = check.value();
            Logger::info("update", "checked: installed " + check.value().currentVersion + ", latest " +
                                       check.value().latestVersion +
                                       (check.value().available ? ", update available" : ", up to date"));
        }
        finished(std::move(check));
    };
    if (!m_releaseBody.isEmpty() && m_lastApiAnswer.isValid() &&
        m_lastApiAnswer.secsTo(QDateTime::currentDateTimeUtc()) < kReuseAnswerSeconds) {
        done(evaluateBody(m_releaseBody));
        return;
    }
    if (apiPaused()) {
        checkWithoutApi(done);
        return;
    }
    HttpRequest request = githubRequest(releasesApiUrl());
    if (!m_releaseEtag.isEmpty()) {
        request.headers.emplace_back("If-None-Match", m_releaseEtag);
    }
    m_http->send(request, [self, done](Result<HttpResponse> response) {
        if (!self) {
            return;
        }
        if (response && response.value().status == 304 && !self->m_releaseBody.isEmpty()) {
            self->m_lastApiAnswer = QDateTime::currentDateTimeUtc();
            Logger::info("update", "no new releases since the last check");
            done(self->evaluateBody(self->m_releaseBody));
            return;
        }
        if (response && response.value().isSuccess()) {
            self->storeReleaseCache(response.value().header("ETag"), response.value().body);
            done(self->evaluateBody(response.value().body));
            return;
        }
        const Error error = httpError(response);
        if (error.code == "UPDATE_OFFLINE") {
            done(error);
            return;
        }
        if (error.code == "UPDATE_RATE_LIMITED") {
            self->m_apiBlockedUntil = QDateTime::currentDateTimeUtc().addSecs(kApiPauseSeconds);
            Logger::warning("update", "GitHub API limit reached, using the release page for 15 minutes");
        }
        Logger::warning("update", "GitHub API unavailable (" + error.details + "), checking the release page instead");
        self->checkWithoutApi(done);
    });
}

void UpdateService::checkWithoutApi(std::function<void(Result<VersionCheck>)> done)
{
    QPointer<UpdateService> self(this);
    m_http->send(plainRequest(latestReleasePageUrl(), "HEAD"), [self, done](Result<HttpResponse> response) {
        if (!self) {
            return;
        }
        if (!response || !response.value().isSuccess()) {
            done(httpError(response));
            return;
        }
        const std::string version = versionFromReleaseUrl(fromQString(response.value().url.toString()));
        if (version.empty()) {
            done(Error{"UPDATE_NOT_PUBLISHED", kNotPublished, "the repository has no releases"});
            return;
        }
        self->fetchChangelog([self, version, done](Result<std::vector<VersionNotes>> changelog) {
            if (!self) {
                return;
            }
            done(evaluateLatestTag(version, self->m_currentVersion,
                                   changelog ? std::move(changelog.value()) : std::vector<VersionNotes>{},
                                   self->m_installKind));
        });
    });
}

void UpdateService::fetchChangelog(std::function<void(Result<std::vector<VersionNotes>>)> done)
{
    QPointer<UpdateService> self(this);
    m_http->send(plainRequest(changelogUrl()), [self, done](Result<HttpResponse> response) {
        if (!self) {
            return;
        }
        if (!response || !response.value().isSuccess()) {
            done(Error{"CHANGELOG_FAILED", "Failed to load changelog", {}});
            return;
        }
        const QByteArray &body = response.value().body;
        done(parseChangelog(std::string_view(body.constData(), static_cast<std::size_t>(body.size()))));
    });
}

void UpdateService::performUpdate(const VersionCheck &check)
{
    if (m_updating || m_ready || !check.available || !check.package) {
        return;
    }
    m_updating = true;
    m_progress = 0;
    emit progressChanged(0);

    const std::filesystem::path updateDirectory = m_appDirectory / "update";
    (void)removeDir(updateDirectory);
    if (auto made = ensureDir(updateDirectory); !made) {
        fail(made.error().details);
        return;
    }
    writeUpdateInfo(UpdateInfo{m_currentVersion, check.latestVersion,
                               QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(), true,
                               check.notes});

    const UpdatePackage package = *check.package;
    const std::filesystem::path file = updateDirectory / std::filesystem::path(toWide(fileNameFromUrl(package.url)));
    const bool installer = package.kind == PackageKind::Installer;
    QPointer<UpdateService> self(this);
    m_downloader->download(
        QUrl(toQString(package.url)), file, kMaxDownloadBytes,
        [self](int percent) {
            if (self) {
                self->m_progress = percent;
                emit self->progressChanged(percent);
            }
        },
        [self, file, installer, package](Result<void> downloaded) {
            if (!self) {
                return;
            }
            if (!downloaded) {
                self->fail(downloaded.error().details);
                return;
            }
            QThreadPool::globalInstance()->start([self, file, installer, package] {
                const bool verified = verifyDownload(file, package);
                QMetaObject::invokeMethod(
                    self.data(),
                    [self, file, installer, verified] {
                        if (!self) {
                            return;
                        }
                        if (!verified) {
                            (void)safeDelete(file);
                            self->fail("downloaded file failed the integrity check");
                            return;
                        }
                        self->finishInstall(file, installer);
                    },
                    Qt::QueuedConnection);
            });
        });
}

void UpdateService::finishInstall(const std::filesystem::path &file, bool installer)
{
    const auto succeed = [this] {
        m_updating = false;
        m_ready = true;
        m_progress = 100;
        emit progressChanged(100);
        emit updateReady();
    };
    if (installer) {
        m_pendingInstaller = file;
        succeed();
        return;
    }
    QPointer<UpdateService> self(this);
    const std::filesystem::path appDirectory = m_appDirectory;
    QThreadPool::globalInstance()->start([self, file, appDirectory, succeed] {
        const std::filesystem::path staging = file.parent_path() / "staging";
        Result<void> outcome;
        if (auto extracted = extractZip(file, staging); !extracted) {
            outcome = extracted.error();
        } else if (auto applied = applyStagedFiles(staging, appDirectory, appDirectory / kCleanupFile); !applied) {
            outcome = applied.error();
        }
        (void)safeDelete(file);
        QMetaObject::invokeMethod(
            self.data(),
            [self, outcome, succeed] {
                if (!self) {
                    return;
                }
                if (outcome) {
                    succeed();
                } else {
                    self->fail(outcome.error().details);
                }
            },
            Qt::QueuedConnection);
    });
}

void UpdateService::fail(const std::string &details)
{
    Logger::error("update", details);
    m_updating = false;
    if (auto info = pendingChangelog()) {
        info->showChangelog = false;
        writeUpdateInfo(*info);
    }
    emit updateFailed(QStringLiteral("The update could not be downloaded. Please try again later."));
}

void UpdateService::writeUpdateInfo(const UpdateInfo &info) const
{
    OrderedJson json = OrderedJson::object();
    json["previousVersion"] = info.previousVersion;
    json["newVersion"] = info.newVersion;
    json["updateTime"] = info.updateTime;
    json["showChangelog"] = info.showChangelog;
    json["notes"] = info.notes;
    (void)atomicWrite(m_configDirectory / kUpdateInfoFile, dumpJson(json, 2));
}

std::optional<UpdateInfo> UpdateService::pendingChangelog() const
{
    auto text = readTextFile(m_configDirectory / kUpdateInfoFile);
    if (!text) {
        return std::nullopt;
    }
    auto parsed = parseJson<Json>(text.value());
    if (!parsed || !parsed.value().is_object() || !parsed.value().value("showChangelog", false)) {
        return std::nullopt;
    }
    const Json &json = parsed.value();
    return UpdateInfo{json.value("previousVersion", std::string()), json.value("newVersion", std::string()),
                      json.value("updateTime", std::string()), true, json.value("notes", std::string())};
}

void UpdateService::markChangelogShown()
{
    if (auto info = pendingChangelog()) {
        info->showChangelog = false;
        writeUpdateInfo(*info);
    }
}

Result<std::vector<std::filesystem::path>> UpdateService::applyStagedFiles(const std::filesystem::path &staging,
                                                                           const std::filesystem::path &appDirectory,
                                                                           const std::filesystem::path &cleanupList)
{
    std::error_code error;
    std::vector<std::filesystem::path> replaced;
    std::vector<std::string> leftovers;
    for (auto it = std::filesystem::recursive_directory_iterator(staging, error);
         it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
        if (error) {
            return Error{"UPDATE_APPLY_FAILED", "Unable to complete update.", error.message()};
        }
        if (!it->is_regular_file(error)) {
            continue;
        }
        const std::filesystem::path relative = std::filesystem::relative(it->path(), staging, error);
        const std::string top = relative.begin()->string();
        if (std::find(kPreservedFolders.begin(), kPreservedFolders.end(), toLowerAscii(top)) != kPreservedFolders.end()) {
            continue;
        }
        const std::filesystem::path target = appDirectory / relative;
        std::filesystem::create_directories(target.parent_path(), error);
        if (std::filesystem::exists(target, error) && !std::filesystem::remove(target, error)) {
            std::filesystem::path parked = target;
            parked += L".old";
            for (int n = 1; std::filesystem::exists(parked, error); ++n) {
                parked = target;
                parked += L".old" + std::to_wstring(n);
            }
            if (!MoveFileExW(target.c_str(), parked.c_str(), MOVEFILE_REPLACE_EXISTING)) {
                return Error{"UPDATE_APPLY_FAILED", "Unable to complete update.", "cannot replace " + target.filename().string()};
            }
            leftovers.push_back(fromWide(parked.wstring()));
        }
        if (!MoveFileExW(it->path().c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
            return Error{"UPDATE_APPLY_FAILED", "Unable to complete update.", "cannot write " + target.filename().string()};
        }
        replaced.push_back(target);
    }
    if (!leftovers.empty()) {
        Json list = Json::array();
        if (auto existing = readTextFile(cleanupList)) {
            if (auto parsed = parseJson<Json>(existing.value()); parsed && parsed.value().is_array()) {
                list = parsed.value();
            }
        }
        for (const std::string &file : leftovers) {
            list.push_back(file);
        }
        (void)atomicWrite(cleanupList, dumpJson(list));
    }
    std::filesystem::remove_all(staging, error);
    return replaced;
}

void UpdateService::cleanupPendingFiles()
{
    const std::filesystem::path cleanupList = m_appDirectory / kCleanupFile;
    const std::filesystem::path updateDirectory = m_appDirectory / "update";
    QThreadPool::globalInstance()->start([cleanupList, updateDirectory] {
        std::error_code error;
        if (auto text = readTextFile(cleanupList)) {
            Json remaining = Json::array();
            if (auto parsed = parseJson<Json>(text.value()); parsed && parsed.value().is_array()) {
                for (const Json &entry : parsed.value()) {
                    if (!entry.is_string()) {
                        continue;
                    }
                    const std::filesystem::path file(toWide(entry.get<std::string>()));
                    if (std::filesystem::exists(file, error) && !std::filesystem::remove(file, error)) {
                        remaining.push_back(entry);
                    }
                }
            }
            if (remaining.empty()) {
                std::filesystem::remove(cleanupList, error);
            } else {
                (void)atomicWrite(cleanupList, dumpJson(remaining));
            }
        }
        std::filesystem::remove_all(updateDirectory, error);
    });
}

bool UpdateService::launchPendingInstaller()
{
    if (m_pendingInstaller.empty()) {
        return false;
    }
    std::wstring command = L"\"" + m_pendingInstaller.wstring() + L"\" /SILENT /SUPPRESSMSGBOXES /NORESTART";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr, nullptr, &startup,
                       &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        m_pendingInstaller.clear();
        return true;
    }
    Logger::error("update", "failed to launch installer");
    m_pendingInstaller.clear();
    return false;
}

}  // namespace sky
