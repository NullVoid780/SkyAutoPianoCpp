#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "services/library/legacy_import.hpp"

namespace sky {

// The legacy Electron app is identified ONLY by its Inno Setup AppId. The
// C++ app's own AppId is listed here so tests can prove it is never selected;
// display names are never used for selection because both apps share the
// name "Sky Auto Piano" and the executable name "SkyAutoPiano.exe".
inline constexpr const char *kLegacyElectronAppId = "D6AC6716-EBED-4150-ACAC-686A6AA31FAB";
inline constexpr const char *kCppAppId = "6C2E4F7A-3B1D-4E8F-9A5C-2D7B8E1F4A60";

// One uninstall-registry record for an exact "{AppId}_is1" key, tagged with
// the hive/view it came from. displayName is informational only.
struct LegacyUninstallEntry {
    std::string source;
    std::string uninstallString;
    std::string installLocation;
    std::string displayName;
};

// System boundary for legacy removal. Production uses Win32 registry /
// process APIs and QProcess; tests inject fakes, so no test ever touches a
// real registry, a real process list, or a real uninstaller.
class ILegacyRemovalSystem {
public:
    virtual ~ILegacyRemovalSystem() = default;

    // Every entry registered under the exact "{appId}_is1" uninstall key,
    // across registry hives and views. Never a name-based search.
    virtual std::vector<LegacyUninstallEntry> uninstallEntries(const std::string &appId) const = 0;
    // Canonical path with reparse points resolved; empty when unresolvable.
    virtual std::filesystem::path canonicalPath(const std::filesystem::path &path) const = 0;
    // Full executable paths of all currently running processes.
    virtual std::vector<std::filesystem::path> runningProcessExecutables() const = 0;
    // Launch exe + argv directly (no shell, no command string). Returns
    // false when the process could not be started.
    virtual bool launch(const std::filesystem::path &exe, const std::vector<std::string> &args) = 0;
};

std::shared_ptr<ILegacyRemovalSystem> makeWin32LegacyRemovalSystem();

// A legacy installation that passed every validation in section 6.2 of the
// migration plan: exact AppId, parseable UninstallString resolving to an
// existing Inno uninstaller inside the legacy tree, plausible Electron
// layout, and no overlap with the C++ installation/data trees.
struct LegacyInstallInfo {
    LegacyInstall legacy;
    std::filesystem::path installRoot;
    std::filesystem::path executable;
    std::filesystem::path uninstaller;
    std::vector<std::string> uninstallerArgs;
    std::string sourceFingerprint;
    std::string registrySource;
};

enum class LegacyDetectionStatus { Found, NotFound, Ambiguous, Invalid };

struct LegacyDetection {
    LegacyDetectionStatus status = LegacyDetectionStatus::NotFound;
    std::optional<LegacyInstallInfo> install;
    // Internal explanation for logs/reports, and the manual path offered to
    // the user whenever automatic removal is not possible.
    std::string reason;
    std::string manualGuidance;
};

// Evidence about the Phase 1 import, assembled by the application from the
// persisted migration state plus a fresh reconciliation re-verification.
// The removal gate never trusts a bare "library is non-empty" signal.
struct LegacyImportEvidence {
    std::string migrationStatus;
    std::string sourceFingerprint;
    bool legacyRemoved = false;
    bool hasSeriousFailures = false;
    bool reVerified = false;
    int reVerificationFailed = 0;
};

enum class RemovalBlocker {
    InstallNotValidated,
    AlreadyRemoved,
    ImportNotVerified,
    SourceMismatch,
    SeriousImportIssues,
    ReverificationRequired,
    PathOverlap,
    LegacyRunning,
    RemovalInProgress,
};

struct RemovalGate {
    bool allowed = false;
    std::vector<RemovalBlocker> blockers;
};

enum class RemovalStatus {
    Launched,
    Blocked,
    LaunchFailed,
    AlreadyInProgress,
    Completed,
    Incomplete,
    Failed,
};

struct RemovalOutcome {
    RemovalStatus status = RemovalStatus::Failed;
    std::string reason;
    bool registryEntryGone = false;
    bool libraryIntact = false;
};

// Parses a registry uninstall command with CommandLineToArgvW semantics.
// Returns the executable plus its argument vector, or nullopt when the
// quoting is malformed. Never produces a shell command string.
std::optional<std::pair<std::filesystem::path, std::vector<std::string>>> parseUninstallCommand(
    const std::string &command);

class LegacyRemovalService {
public:
    // libraryIntactCheck re-validates the C++ library after a removal; it is
    // injected so the application decides what "intact" means (and tests can
    // fake it).
    LegacyRemovalService(std::shared_ptr<ILegacyRemovalSystem> system, std::filesystem::path ownAppDirectory,
                         std::filesystem::path ownDataDirectory, std::function<bool()> libraryIntactCheck);

    LegacyDetection detectLegacyInstall() const;
    RemovalGate canOfferRemoval(const LegacyImportEvidence &evidence, const LegacyDetection &detection) const;
    bool isLegacyRunning(const LegacyInstallInfo &install) const;

    // Re-runs the gate before launching. Only a Launched result means the
    // official uninstaller was started; poll with pollRemoval() afterwards.
    RemovalOutcome requestLegacyRemoval(const LegacyInstallInfo &confirmed, const LegacyImportEvidence &evidence);
    // Nullopt while the uninstaller is still running. Once it exits, the
    // outcome is decided by post-checks (registry entry gone AND C++ library
    // intact), never by the uninstaller's exit code.
    std::optional<RemovalOutcome> pollRemoval();
    bool removalInProgress() const { return m_inProgress; }

private:
    bool rootsOverlap(const std::filesystem::path &legacyRoot) const;

    std::shared_ptr<ILegacyRemovalSystem> m_system;
    std::filesystem::path m_ownAppDirectory;
    std::filesystem::path m_ownDataDirectory;
    std::function<bool()> m_libraryIntactCheck;
    bool m_inProgress = false;
    std::optional<LegacyInstallInfo> m_pending;
};

}  // namespace sky
