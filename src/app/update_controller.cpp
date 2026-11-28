#include "app/update_controller.hpp"

#include <QNetworkInformation>
#include <QUrl>

#include <algorithm>
#include <array>
#include <chrono>

#include "core/translator.hpp"
#include "core/logger.hpp"
#include "core/string_utils.hpp"
#include "platform/shell_open.hpp"
#include "platform/tray_notification.hpp"
#include "services/notifications/toast_service.hpp"
#include "ui/settings_window/update_prompt.hpp"

namespace sky {

namespace {

constexpr std::chrono::minutes kTickInterval{10};
constexpr qint64 kCheckEverySeconds = 4 * 60 * 60;
constexpr std::array<std::chrono::seconds, 4> kRetryDelays{std::chrono::seconds{30}, std::chrono::minutes{2},
                                                           std::chrono::minutes{10}, std::chrono::minutes{30}};
constexpr std::chrono::seconds kOnlineDelay{3};

}  // namespace

UpdateController::UpdateController(UpdateService &updates, ToastService &toasts, TrayNotification &tray,
                                   QObject *parent)
    : QObject(parent), m_updates(updates), m_toasts(toasts), m_tray(tray)
{
    m_timer.setInterval(kTickInterval);
    connect(&m_timer, &QTimer::timeout, this, &UpdateController::onTick);
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &UpdateController::checkAndOffer);
    connect(&m_updates, &UpdateService::progressChanged, this, &UpdateController::onProgress);
    connect(&m_updates, &UpdateService::updateReady, this, &UpdateController::onReady);
    connect(&m_updates, &UpdateService::updateFailed, this, &UpdateController::onFailed);
}

void UpdateController::start(QWidget *window, std::function<bool()> enabled)
{
    m_window = window;
    m_enabled = std::move(enabled);
    m_timer.start();
    watchNetwork();
    checkAndOffer();
}

void UpdateController::watchNetwork()
{
    if (!QNetworkInformation::loadDefaultBackend()) {
        Logger::info("update", "network status is not available, using timed retries only");
        return;
    }
    QNetworkInformation *network = QNetworkInformation::instance();
    connect(network, &QNetworkInformation::reachabilityChanged, this, [this](QNetworkInformation::Reachability state) {
        if (state == QNetworkInformation::Reachability::Online) {
            onOnline();
        }
    });
}

void UpdateController::onOnline()
{
    if (m_failures > 0 || !m_lastSuccess.isValid()) {
        m_retry.start(kOnlineDelay);
    }
}

void UpdateController::onTick()
{
    if (!m_lastSuccess.isValid() && m_failures == 0) {
        checkAndOffer();
        return;
    }
    if (m_lastSuccess.isValid() && m_lastSuccess.secsTo(QDateTime::currentDateTimeUtc()) >= kCheckEverySeconds) {
        checkAndOffer();
    }
}

void UpdateController::scheduleRetry()
{
    const std::size_t step = std::min<std::size_t>(static_cast<std::size_t>(m_failures), kRetryDelays.size()) - 1;
    m_retry.start(kRetryDelays[step]);
    Logger::info("update", "will try again in " + std::to_string(kRetryDelays[step].count()) + " seconds");
}

bool UpdateController::promptOpen() const
{
    return std::any_of(m_prompts.begin(), m_prompts.end(),
                       [](const QPointer<UpdatePrompt> &prompt) { return prompt && prompt->isOpen(); });
}

void UpdateController::checkAndOffer()
{
    if (m_checking || !m_window || (m_enabled && !m_enabled()) || m_updates.isUpdating() || m_updates.isReady() ||
        promptOpen()) {
        return;
    }
    m_checking = true;
    QPointer<UpdateController> self(this);
    m_updates.checkForUpdates([self](Result<VersionCheck> check) {
        if (!self) {
            return;
        }
        self->m_checking = false;
        const bool settled = check || check.error().code == "UPDATE_NOT_PUBLISHED";
        if (!settled) {
            ++self->m_failures;
            Logger::info("update", "update check failed: " + check.error().details);
            self->scheduleRetry();
            return;
        }
        self->m_failures = 0;
        self->m_retry.stop();
        self->m_lastSuccess = QDateTime::currentDateTimeUtc();
        if (!check) {
            return;
        }
        const VersionCheck &info = check.value();
        if (!info.available || !self->m_window || self->promptOpen()) {
            return;
        }
        if (!info.critical && self->m_snoozedVersion == info.latestVersion) {
            return;
        }
        self->offer(self->m_window, info);
    });
}

UpdatePrompt *UpdateController::promptFor(QWidget *window)
{
    std::erase_if(m_prompts, [](const QPointer<UpdatePrompt> &prompt) { return prompt.isNull(); });
    for (const QPointer<UpdatePrompt> &prompt : m_prompts) {
        if (prompt->parentWidget() == window) {
            return prompt;
        }
    }
    auto *prompt = new UpdatePrompt(window);
    connect(prompt, &UpdatePrompt::installRequested, this, [this, prompt] {
        prompt->showDownloading(0);
        m_updates.performUpdate(m_check);
    });
    connect(prompt, &UpdatePrompt::restartRequested, this, &UpdateController::restartRequested);
    connect(prompt, &UpdatePrompt::releasePageRequested, this,
            [this] { openExternalUrl(QUrl(toQString(m_check.releasePage))); });
    connect(prompt, &UpdatePrompt::dismissed, this, [this] {
        if (!m_check.critical && !m_check.latestVersion.empty()) {
            m_snoozedVersion = m_check.latestVersion;
        }
    });
    m_prompts.emplace_back(prompt);
    return prompt;
}

void UpdateController::offer(QWidget *window, const VersionCheck &check)
{
    if (!window || !check.available) {
        return;
    }
    m_check = check;
    UpdatePrompt *prompt = promptFor(window);
    const auto missed = missedVersions(check.history, check.currentVersion, check.latestVersion);
    prompt->present(toQString(check.currentVersion), toQString(check.latestVersion),
                    toQString(composeUpdateNotes(check.notes, missed)), check.critical);
    if (m_updates.isReady()) {
        prompt->showReady();
    } else if (m_updates.isUpdating()) {
        prompt->showDownloading(m_updates.progress());
    }
}

void UpdateController::onProgress(int percent)
{
    for (const QPointer<UpdatePrompt> &prompt : m_prompts) {
        if (prompt && prompt->state() == UpdatePrompt::State::Downloading) {
            prompt->showDownloading(percent);
        }
    }
}

void UpdateController::onReady()
{
    for (const QPointer<UpdatePrompt> &prompt : m_prompts) {
        if (prompt && prompt->state() == UpdatePrompt::State::Downloading) {
            prompt->showReady();
        }
    }
    m_tray.show(tr("A new update is ready to install"),
                tr("SkyAutoPianoCpp %1 has been downloaded and will be installed automatically when you "
                               "close the app.")
                    .arg(toQString(m_check.latestVersion)));
}

void UpdateController::onFailed(const QString &message)
{
    bool shown = false;
    for (const QPointer<UpdatePrompt> &prompt : m_prompts) {
        if (prompt && prompt->isOpen()) {
            prompt->showFailed(message);
            shown = true;
        }
    }
    if (!shown) {
        m_toasts.showTitled(ToastType::Error, trUtf8("Update Failed"), fromQString(message), kStickyToast);
    }
}

}  // namespace sky
