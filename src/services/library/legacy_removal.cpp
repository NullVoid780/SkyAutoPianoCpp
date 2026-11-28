#include "services/library/legacy_removal.hpp"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <iterator>
#include <set>
#include <string_view>
#include <system_error>
#include <utility>

#include <QProcess>
#include <QString>
#include <QStringList>

#include "core/file_system.hpp"
#include "core/logger.hpp"
#include "core/string_utils.hpp"

namespace sky {

namespace {

constexpr const char *kManualGuidance =
    "You can remove the old version yourself in Windows Settings > Apps > Installed apps: find the entry whose "
    "uninstall registration matches the old Electron version and choose Uninstall. Nothing is deleted by this app.";

std::string blockerName(RemovalBlocker blocker)
{
    switch (blocker) {
        case RemovalBlocker::InstallNotValidated: return "install-not-validated";
        case RemovalBlocker::AlreadyRemoved: return "already-removed";
        case RemovalBlocker::ImportNotVerified: return "import-not-verified";
        case RemovalBlocker::SourceMismatch: return "source-mismatch";
        case RemovalBlocker::SeriousImportIssues: return "serious-import-issues";
        case RemovalBlocker::ReverificationRequired: return "reverification-required";
        case RemovalBlocker::PathOverlap: return "path-overlap";
        case RemovalBlocker::LegacyRunning: return "legacy-running";
        case RemovalBlocker::RemovalInProgress: return "removal-in-progress";
    }
    return "unknown";
}

bool isInnoUninstallerName(const std::filesystem::path &exe)
{
    const std::string name = toLowerAscii(fromWide(exe.filename().wstring()));
    return name.starts_with("unins") && name.ends_with(".exe");
}

// Comparison key for a path that is expected to exist: resolved through the
// system adapter when possible so junctions/reparse points are honoured.
std::string pathKey(const ILegacyRemovalSystem &system, const std::filesystem::path &path)
{
    std::error_code error;
    std::filesystem::path resolved = system.canonicalPath(path);
    if (resolved.empty()) {
        resolved = std::filesystem::weakly_canonical(path, error);
        if (error) {
            resolved = path;
        }
    }
    return toLowerAscii(fromWide(resolved.wstring()));
}

bool insideOrEqual(const std::string &childKey, const std::string &rootKey)
{
    if (childKey == rootKey) {
        return true;
    }
    return childKey.size() > rootKey.size() && childKey.starts_with(rootKey) && childKey[rootKey.size()] == '\\';
}

// One token of a Windows command line, following CommandLineToArgvW rules.
// Returns false on an unterminated quote (malformed input).
bool parseToken(const std::wstring &text, std::size_t &pos, std::wstring &token)
{
    token.clear();
    bool inQuotes = false;
    bool any = false;
    while (pos < text.size()) {
        const wchar_t c = text[pos];
        if (c == L'\\') {
            std::size_t run = 0;
            while (pos < text.size() && text[pos] == L'\\') {
                ++run;
                ++pos;
            }
            if (pos < text.size() && text[pos] == L'"') {
                token.append(run / 2, L'\\');
                if (run % 2 == 1) {
                    token.push_back(L'"');
                } else {
                    inQuotes = !inQuotes;
                }
                ++pos;
                any = true;
                continue;
            }
            token.append(run, L'\\');
            any = true;
            continue;
        }
        if (c == L'"') {
            if (inQuotes && pos + 1 < text.size() && text[pos + 1] == L'"') {
                token.push_back(L'"');
                pos += 2;
                any = true;
                continue;
            }
            inQuotes = !inQuotes;
            ++pos;
            any = true;
            continue;
        }
        if (!inQuotes && (c == L' ' || c == L'\t')) {
            break;
        }
        token.push_back(c);
        ++pos;
        any = true;
    }
    return any && !inQuotes;
}

}  // namespace

std::optional<std::pair<std::filesystem::path, std::vector<std::string>>> parseUninstallCommand(
    const std::string &command)
{
    const std::wstring text = toWide(command);
    std::vector<std::wstring> tokens;
    std::size_t pos = 0;
    while (true) {
        while (pos < text.size() && (text[pos] == L' ' || text[pos] == L'\t')) {
            ++pos;
        }
        if (pos >= text.size()) {
            break;
        }
        std::wstring token;
        if (!parseToken(text, pos, token)) {
            return std::nullopt;
        }
        tokens.push_back(std::move(token));
    }
    if (tokens.empty() || tokens.front().empty()) {
        return std::nullopt;
    }
    std::vector<std::string> args;
    for (std::size_t i = 1; i < tokens.size(); ++i) {
        args.push_back(fromWide(tokens[i]));
    }
    return std::make_pair(std::filesystem::path(tokens.front()), std::move(args));
}

// ---------------------------------------------------------------------------
// Win32 production adapter
// ---------------------------------------------------------------------------

namespace {

std::string readRegistryString(HKEY key, const wchar_t *name)
{
    wchar_t buffer[4096];
    DWORD size = static_cast<DWORD>(sizeof(buffer));
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS) {
        return {};
    }
    return fromWide(buffer);
}

class Win32LegacyRemovalSystem final : public ILegacyRemovalSystem {
public:
    std::vector<LegacyUninstallEntry> uninstallEntries(const std::string &appId) const override
    {
        const std::wstring subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" +
                                    toWide("{" + appId + "}_is1");
        struct View {
            HKEY hive;
            const char *name;
            DWORD flags;
        };
        const std::array<View, 4> views = {{
            {HKEY_CURRENT_USER, "HKCU", KEY_WOW64_64KEY},
            {HKEY_CURRENT_USER, "HKCU (32-bit view)", KEY_WOW64_32KEY},
            {HKEY_LOCAL_MACHINE, "HKLM", KEY_WOW64_64KEY},
            {HKEY_LOCAL_MACHINE, "HKLM (32-bit view)", KEY_WOW64_32KEY},
        }};
        std::vector<LegacyUninstallEntry> entries;
        std::set<std::string> seen;
        for (const View &view : views) {
            HKEY key = nullptr;
            if (RegOpenKeyExW(view.hive, subKey.c_str(), 0, KEY_READ | view.flags, &key) != ERROR_SUCCESS) {
                continue;
            }
            LegacyUninstallEntry entry;
            entry.source = view.name;
            entry.uninstallString = readRegistryString(key, L"UninstallString");
            entry.installLocation = readRegistryString(key, L"InstallLocation");
            entry.displayName = readRegistryString(key, L"DisplayName");
            RegCloseKey(key);
            const std::string identity = entry.uninstallString + '\x1f' + entry.installLocation;
            if (seen.insert(identity).second) {
                entries.push_back(std::move(entry));
            }
        }
        return entries;
    }

    std::filesystem::path canonicalPath(const std::filesystem::path &path) const override
    {
        const std::wstring wide = path.wstring();
        HANDLE handle = CreateFileW(wide.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            return {};
        }
        wchar_t buffer[32768];
        const DWORD length = GetFinalPathNameByHandleW(handle, buffer, static_cast<DWORD>(std::size(buffer)),
                                                       FILE_NAME_NORMALIZED);
        CloseHandle(handle);
        if (length == 0 || length >= std::size(buffer)) {
            return {};
        }
        std::wstring resolved(buffer, length);
        constexpr std::wstring_view kPrefix = L"\\\\?\\";
        constexpr std::wstring_view kUncPrefix = L"\\\\?\\UNC\\";
        if (resolved.starts_with(kUncPrefix)) {
            resolved = L"\\\\" + resolved.substr(kUncPrefix.size());
        } else if (resolved.starts_with(kPrefix)) {
            resolved = resolved.substr(kPrefix.size());
        }
        return std::filesystem::path(resolved);
    }

    std::vector<std::filesystem::path> runningProcessExecutables() const override
    {
        std::vector<std::filesystem::path> paths;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return paths;
        }
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
                if (!process) {
                    continue;
                }
                wchar_t buffer[32768];
                DWORD size = static_cast<DWORD>(std::size(buffer));
                if (QueryFullProcessImageNameW(process, 0, buffer, &size)) {
                    paths.emplace_back(std::wstring(buffer, size));
                }
                CloseHandle(process);
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return paths;
    }

    bool launch(const std::filesystem::path &exe, const std::vector<std::string> &args) override
    {
        QStringList arguments;
        for (const std::string &arg : args) {
            arguments.push_back(toQString(arg));
        }
        const std::filesystem::path directory = exe.parent_path();
        return QProcess::startDetached(toQString(fromWide(exe.wstring())), arguments,
                                       toQString(fromWide(directory.wstring())));
    }
};

}  // namespace

std::shared_ptr<ILegacyRemovalSystem> makeWin32LegacyRemovalSystem()
{
    return std::make_shared<Win32LegacyRemovalSystem>();
}

// ---------------------------------------------------------------------------
// Service
// ---------------------------------------------------------------------------

LegacyRemovalService::LegacyRemovalService(std::shared_ptr<ILegacyRemovalSystem> system,
                                           std::filesystem::path ownAppDirectory,
                                           std::filesystem::path ownDataDirectory,
                                           std::function<bool()> libraryIntactCheck)
    : m_system(std::move(system)),
      m_ownAppDirectory(std::move(ownAppDirectory)),
      m_ownDataDirectory(std::move(ownDataDirectory)),
      m_libraryIntactCheck(std::move(libraryIntactCheck))
{
}

bool LegacyRemovalService::rootsOverlap(const std::filesystem::path &legacyRoot) const
{
    const std::string legacyKey = pathKey(*m_system, legacyRoot);
    for (const std::filesystem::path &own : {m_ownAppDirectory, m_ownDataDirectory}) {
        const std::string ownKey = pathKey(*m_system, own);
        if (insideOrEqual(legacyKey, ownKey) || insideOrEqual(ownKey, legacyKey)) {
            return true;
        }
    }
    return false;
}

LegacyDetection LegacyRemovalService::detectLegacyInstall() const
{
    LegacyDetection detection;
    detection.manualGuidance = kManualGuidance;
    const std::vector<LegacyUninstallEntry> entries = m_system->uninstallEntries(kLegacyElectronAppId);
    if (entries.empty()) {
        detection.status = LegacyDetectionStatus::NotFound;
        detection.reason = "no uninstall entry is registered for the exact legacy AppId";
        return detection;
    }

    struct Candidate {
        const LegacyUninstallEntry *entry;
        std::filesystem::path exe;
        std::vector<std::string> args;
        std::filesystem::path root;
    };
    std::vector<Candidate> candidates;
    std::string parseFailure;
    for (const LegacyUninstallEntry &entry : entries) {
        auto parsed = parseUninstallCommand(entry.uninstallString);
        if (!parsed) {
            parseFailure = "the registered UninstallString could not be parsed safely";
            continue;
        }
        Candidate candidate;
        candidate.entry = &entry;
        candidate.exe = parsed->first;
        candidate.args = std::move(parsed->second);
        candidate.root = entry.installLocation.empty()
                             ? candidate.exe.parent_path()
                             : std::filesystem::path(toWide(entry.installLocation));
        candidates.push_back(std::move(candidate));
    }
    if (candidates.empty()) {
        detection.status = LegacyDetectionStatus::Invalid;
        detection.reason = parseFailure;
        return detection;
    }

    const std::string firstRootKey = pathKey(*m_system, candidates.front().root);
    for (const Candidate &candidate : candidates) {
        if (pathKey(*m_system, candidate.root) != firstRootKey) {
            detection.status = LegacyDetectionStatus::Ambiguous;
            detection.reason = "multiple different installations are registered for the legacy AppId";
            return detection;
        }
    }

    const Candidate &candidate = candidates.front();
    auto invalid = [&detection](std::string reason) {
        detection.status = LegacyDetectionStatus::Invalid;
        detection.reason = std::move(reason);
        return detection;
    };

    if (!isInnoUninstallerName(candidate.exe)) {
        return invalid("the registered uninstaller is not an Inno Setup uninstaller (unins*.exe)");
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(candidate.exe, error)) {
        return invalid("the registered uninstaller no longer exists (stale entry)");
    }
    const std::string rootKey = pathKey(*m_system, candidate.root);
    const std::string exeKey = pathKey(*m_system, candidate.exe);
    if (!insideOrEqual(exeKey, rootKey)) {
        return invalid("the registered uninstaller is outside the legacy installation tree");
    }
    const std::string ownAppKey = pathKey(*m_system, m_ownAppDirectory);
    const std::string ownDataKey = pathKey(*m_system, m_ownDataDirectory);
    if (insideOrEqual(exeKey, ownAppKey) || insideOrEqual(exeKey, ownDataKey)) {
        return invalid("the registered uninstaller points inside this app's own tree");
    }
    if (rootsOverlap(candidate.root)) {
        return invalid("the legacy and C++ installation/data roots overlap");
    }
    const auto inspected = inspectLegacyFolder(candidate.root);
    if (!inspected) {
        return invalid("the registered location does not look like the Electron app layout");
    }
    const std::filesystem::path executable = candidate.root / "SkyAutoPiano.exe";
    if (!std::filesystem::is_regular_file(executable, error)) {
        return invalid("the Electron executable is missing from the registered location");
    }

    LegacyInstallInfo info;
    info.legacy = *inspected;
    info.installRoot = candidate.root;
    info.executable = executable;
    info.uninstaller = candidate.exe;
    info.uninstallerArgs = candidate.args;
    info.sourceFingerprint = legacySourceFingerprint(*inspected);
    info.registrySource = candidate.entry->source;
    detection.status = LegacyDetectionStatus::Found;
    detection.install = std::move(info);
    detection.reason = "exact legacy AppId entry validated";
    return detection;
}

bool LegacyRemovalService::isLegacyRunning(const LegacyInstallInfo &install) const
{
    const std::string target = pathKey(*m_system, install.executable);
    for (const std::filesystem::path &running : m_system->runningProcessExecutables()) {
        if (pathKey(*m_system, running) == target) {
            return true;
        }
    }
    return false;
}

RemovalGate LegacyRemovalService::canOfferRemoval(const LegacyImportEvidence &evidence,
                                                  const LegacyDetection &detection) const
{
    RemovalGate gate;
    if (detection.status != LegacyDetectionStatus::Found || !detection.install) {
        gate.blockers.push_back(RemovalBlocker::InstallNotValidated);
        return gate;
    }
    const LegacyInstallInfo &install = *detection.install;
    if (evidence.legacyRemoved) {
        gate.blockers.push_back(RemovalBlocker::AlreadyRemoved);
    }
    if (evidence.migrationStatus != "verified") {
        gate.blockers.push_back(RemovalBlocker::ImportNotVerified);
    } else if (evidence.sourceFingerprint != install.sourceFingerprint) {
        gate.blockers.push_back(RemovalBlocker::SourceMismatch);
    }
    if (evidence.hasSeriousFailures) {
        gate.blockers.push_back(RemovalBlocker::SeriousImportIssues);
    }
    if (!evidence.reVerified || evidence.reVerificationFailed > 0) {
        gate.blockers.push_back(RemovalBlocker::ReverificationRequired);
    }
    const std::string exeKey = pathKey(*m_system, install.uninstaller);
    if (rootsOverlap(install.installRoot) || insideOrEqual(exeKey, pathKey(*m_system, m_ownAppDirectory)) ||
        insideOrEqual(exeKey, pathKey(*m_system, m_ownDataDirectory))) {
        gate.blockers.push_back(RemovalBlocker::PathOverlap);
    }
    if (isLegacyRunning(install)) {
        gate.blockers.push_back(RemovalBlocker::LegacyRunning);
    }
    if (m_inProgress) {
        gate.blockers.push_back(RemovalBlocker::RemovalInProgress);
    }
    gate.allowed = gate.blockers.empty();
    return gate;
}

RemovalOutcome LegacyRemovalService::requestLegacyRemoval(const LegacyInstallInfo &confirmed,
                                                          const LegacyImportEvidence &evidence)
{
    if (m_inProgress) {
        return {RemovalStatus::AlreadyInProgress, "a legacy removal is already in progress", false, false};
    }
    LegacyDetection detection;
    detection.status = LegacyDetectionStatus::Found;
    detection.install = confirmed;
    const RemovalGate gate = canOfferRemoval(evidence, detection);
    if (!gate.allowed) {
        std::string reason = "removal is blocked:";
        for (const RemovalBlocker blocker : gate.blockers) {
            reason += " " + blockerName(blocker);
        }
        return {RemovalStatus::Blocked, reason, false, false};
    }
    if (!m_system->launch(confirmed.uninstaller, confirmed.uninstallerArgs)) {
        Logger::error("legacy-removal", "failed to launch the legacy uninstaller");
        return {RemovalStatus::LaunchFailed, "the legacy uninstaller could not be started", false, false};
    }
    m_inProgress = true;
    m_pending = confirmed;
    Logger::info("legacy-removal", "legacy uninstaller launched");
    return {RemovalStatus::Launched, "the legacy uninstaller was launched", false, false};
}

std::optional<RemovalOutcome> LegacyRemovalService::pollRemoval()
{
    if (!m_inProgress || !m_pending) {
        return std::nullopt;
    }
    const std::string uninstallerKey = pathKey(*m_system, m_pending->uninstaller);
    for (const std::filesystem::path &running : m_system->runningProcessExecutables()) {
        if (pathKey(*m_system, running) == uninstallerKey) {
            return std::nullopt;
        }
    }
    const bool registryGone = m_system->uninstallEntries(kLegacyElectronAppId).empty();
    const bool intact = m_libraryIntactCheck ? m_libraryIntactCheck() : false;
    m_inProgress = false;
    m_pending.reset();
    if (registryGone && intact) {
        return RemovalOutcome{RemovalStatus::Completed, "legacy entry removed and the C++ library is intact", true,
                              true};
    }
    if (!registryGone) {
        return RemovalOutcome{RemovalStatus::Incomplete,
                              "the legacy uninstall entry is still registered; removal did not finish", false,
                              intact};
    }
    return RemovalOutcome{RemovalStatus::Failed, "the C++ library check failed after removal", true, false};
}

}  // namespace sky
