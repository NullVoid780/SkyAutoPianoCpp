#include "services/library/legacy_import.hpp"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <system_error>

#include "core/file_system.hpp"
#include "core/translator.hpp"
#include "core/string_utils.hpp"
#include "core/text_file.hpp"
#include "services/library/chromium_local_storage.hpp"

namespace sky {

namespace {

constexpr int kScanDepth = 2;
constexpr std::array<const wchar_t *, 2> kRegistryHints = {
    L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache",
    L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Store",
};

std::string normalized(const std::filesystem::path &path)
{
    std::error_code error;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    return toLowerAscii(fromWide((error ? path : canonical).wstring()));
}

std::optional<OrderedJson> readJson(const std::filesystem::path &file)
{
    auto text = readTextFile(file);
    if (!text) {
        return std::nullopt;
    }
    auto parsed = parseJson<OrderedJson>(text.value());
    if (!parsed) {
        return std::nullopt;
    }
    return parsed.value();
}

std::optional<std::string> electronVersion(const std::filesystem::path &appRoot)
{
    const auto package = readJson(appRoot / "package.json");
    if (!package || !package->is_object()) {
        return std::nullopt;
    }
    const std::string name = toLowerAscii(package->value("name", std::string()));
    const bool electronMain = package->value("main", std::string()) == "src/main/app.js";
    if (name != "skyautopiano" && name != "sky-auto-piano" && !electronMain) {
        return std::nullopt;
    }
    return package->value("version", std::string());
}

void addKnownFolder(std::vector<std::filesystem::path> &roots, REFKNOWNFOLDERID id)
{
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &path))) {
        roots.emplace_back(path);
    }
    CoTaskMemFree(path);
}

void collectRegistryHints(std::vector<std::filesystem::path> &folders)
{
    for (const wchar_t *keyPath : kRegistryHints) {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
            continue;
        }
        wchar_t name[2048];
        for (DWORD index = 0;; ++index) {
            DWORD length = static_cast<DWORD>(std::size(name));
            if (RegEnumValueW(key, index, name, &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
                break;
            }
            std::wstring value(name, length);
            const std::string lowered = toLowerAscii(fromWide(value));
            const bool sky = lowered.find("skyautopiano") != std::string::npos ||
                             lowered.find("sky auto piano") != std::string::npos ||
                             lowered.find("sky-auto-piano") != std::string::npos;
            if (!sky && lowered.find("\\node_modules\\electron\\dist\\electron.exe") == std::string::npos) {
                continue;
            }
            const std::size_t exe = lowered.find(".exe");
            if (exe == std::string::npos) {
                continue;
            }
            const std::filesystem::path executable(value.substr(0, exe + 4));
            folders.push_back(executable.parent_path());
            const std::size_t modules = lowered.find("\\node_modules\\");
            if (modules != std::string::npos) {
                folders.emplace_back(value.substr(0, modules));
            }
        }
        RegCloseKey(key);
    }
}

bool sameFileBytes(const std::filesystem::path &a, const std::filesystem::path &b)
{
    const auto bytesA = readFileBytes(a);
    const auto bytesB = readFileBytes(b);
    return bytesA && bytesB && bytesA.value() == bytesB.value();
}

void scanFolder(const std::filesystem::path &folder, int depth, std::vector<std::filesystem::path> &candidates)
{
    std::error_code error;
    candidates.push_back(folder);
    if (depth <= 0) {
        return;
    }
    for (const auto &entry : std::filesystem::directory_iterator(
             folder, std::filesystem::directory_options::skip_permission_denied, error)) {
        if (entry.is_directory(error) && !entry.is_symlink(error)) {
            scanFolder(entry.path(), depth - 1, candidates);
        }
    }
}

}  // namespace

std::string legacySongKey(const OrderedJson &row)
{
    const auto text = [&row](const char *key) {
        const auto found = row.find(key);
        return found != row.end() && found->is_string() ? toLowerAscii(trimJs(found->get<std::string>())) : std::string();
    };
    return text("name") + '\x1f' + text("author") + '\x1f' + text("transcribedBy");
}

std::optional<LegacyInstall> inspectLegacyFolder(const std::filesystem::path &folder)
{
    std::error_code error;
    const std::array<std::pair<std::filesystem::path, std::filesystem::path>, 3> layouts = {{
        {folder / "resources" / "app", folder / "resources" / "app" / "data"},
        {folder, folder / "data"},
        {folder.parent_path(), folder},
    }};
    for (const auto &[appRoot, data] : layouts) {
        if (!std::filesystem::is_regular_file(data / "listSheet.json", error)) {
            continue;
        }
        const auto version = electronVersion(appRoot);
        if (!version) {
            continue;
        }
        const auto list = readJson(data / "listSheet.json");
        if (!list || !list->is_array()) {
            continue;
        }
        return LegacyInstall{appRoot, data, *version, static_cast<int>(list->size())};
    }
    return std::nullopt;
}

std::vector<std::filesystem::path> defaultLegacySearchRoots()
{
    std::vector<std::filesystem::path> roots;
    addKnownFolder(roots, FOLDERID_Desktop);
    addKnownFolder(roots, FOLDERID_Downloads);
    addKnownFolder(roots, FOLDERID_Documents);
    addKnownFolder(roots, FOLDERID_UserProgramFiles);
    addKnownFolder(roots, FOLDERID_ProgramFiles);
    addKnownFolder(roots, FOLDERID_ProgramFilesX86);
    addKnownFolder(roots, FOLDERID_Profile);
    return roots;
}

std::vector<LegacyInstall> findLegacyInstalls(const std::filesystem::path &ownDataDirectory,
                                              const std::vector<std::filesystem::path> &searchRoots, bool useRegistryHints)
{
    std::vector<std::filesystem::path> candidates;
    if (useRegistryHints) {
        collectRegistryHints(candidates);
    }
    for (const auto &root : searchRoots) {
        scanFolder(root, kScanDepth, candidates);
    }
    const std::string own = normalized(ownDataDirectory);
    std::vector<std::string> seen;
    std::vector<LegacyInstall> installs;
    for (const auto &candidate : candidates) {
        auto install = inspectLegacyFolder(candidate);
        if (!install) {
            continue;
        }
        const std::string key = normalized(install->dataDirectory);
        if (key == own || std::find(seen.begin(), seen.end(), key) != seen.end()) {
            continue;
        }
        seen.push_back(key);
        installs.push_back(std::move(*install));
    }
    std::stable_sort(installs.begin(), installs.end(),
                     [](const LegacyInstall &a, const LegacyInstall &b) { return a.songCount > b.songCount; });
    return installs;
}

LegacyExtras readLegacyExtras(const LegacyInstall &install, const std::filesystem::path &localStorageDirectory)
{
    LegacyExtras extras;
    if (auto config = readJson(install.root / "config" / "config.json"); config && config->is_object()) {
        extras.config = std::move(*config);
    }
    if (auto playlists = readJson(install.dataDirectory / "playlists.json"); playlists && playlists->is_array()) {
        extras.playlists = std::move(*playlists);
    }
    if (localStorageDirectory.empty()) {
        return extras;
    }
    const auto storage = readChromiumLocalStorage(localStorageDirectory, "file://");
    const auto names = [&storage](const char *key) {
        std::vector<std::string> result;
        const auto found = storage.find(key);
        if (found == storage.end()) {
            return result;
        }
        auto parsed = parseJson<OrderedJson>(found->second);
        if (!parsed || !parsed.value().is_array()) {
            return result;
        }
        for (const OrderedJson &item : parsed.value()) {
            if (item.is_string() && !item.get<std::string>().empty()) {
                result.push_back(item.get<std::string>());
            }
        }
        return result;
    };
    extras.favorites = names("favorites");
    extras.recentPlays = names("recentPlays");
    if (const auto theme = storage.find("theme"); theme != storage.end() &&
                                                  (theme->second == "dark" || theme->second == "light")) {
        extras.theme = theme->second;
    }
    return extras;
}

Result<LegacyImportPlan> planLegacyImport(const LegacyInstall &install, const std::set<std::string> &existingKeys,
                                          const std::filesystem::path &localStorageDirectory,
                                          const std::filesystem::path &destDataDirectory,
                                          const std::map<std::string, std::string> &existingPayloads)
{
    const auto list = readJson(install.dataDirectory / "listSheet.json");
    if (!list || !list->is_array()) {
        return Error{"LEGACY_INVALID", trUtf8("That folder does not contain songs from Sky Auto Piano."), {}};
    }
    LegacyImportPlan plan;
    plan.install = install;
    std::set<std::string> planned;
    for (const OrderedJson &row : *list) {
        if (!row.is_object() || !row.contains("keyMap") || !row["keyMap"].is_string()) {
            ++plan.invalidRows;
            continue;
        }
        plan.allRows.push_back(row);
        const std::string key = legacySongKey(row);
        if (!planned.insert(key).second) {
            ++plan.alreadyPresent;
            continue;
        }
        if (existingKeys.contains(key)) {
            const auto existing = existingPayloads.find(key);
            if (!destDataDirectory.empty() && existing != existingPayloads.end()) {
                // Decide "already present" by payload bytes, not by title: a
                // same-key row with different bytes is a different arrangement
                // and must surface as a conflict instead of a silent skip.
                const std::string sourceFile = row["keyMap"].get<std::string>();
                const std::filesystem::path sourcePath = install.dataDirectory / toWide(sourceFile);
                std::error_code error;
                if (sourceFile.find_first_of("/\\") != std::string::npos ||
                    !std::filesystem::is_regular_file(sourcePath, error)) {
                    ++plan.missingPayloads;
                    continue;
                }
                const std::filesystem::path destPath = destDataDirectory / toWide(existing->second);
                if (std::filesystem::is_regular_file(destPath, error) && sameFileBytes(sourcePath, destPath)) {
                    ++plan.alreadyPresent;
                } else {
                    plan.conflicts.push_back(LegacySheetConflict{row, sourceFile, existing->second});
                }
                continue;
            }
            ++plan.alreadyPresent;
            continue;
        }
        plan.rows.push_back(row);
    }
    plan.extras = readLegacyExtras(install, localStorageDirectory);
    return plan;
}

OrderedJson LegacyImportResult::toJson() const
{
    OrderedJson json = OrderedJson::object();
    json["version"] = 1;
    json["sourceFingerprint"] = sourceFingerprint;
    json["sourceAccess"] = sourceAccess;
    json["sheets"] = OrderedJson{{"planned", sheets.planned}, {"copied", sheets.copied},
                                 {"alreadyPresent", sheets.alreadyPresent}, {"conflicts", sheets.conflicts},
                                 {"failed", sheets.failed}, {"validated", sheets.validated}};
    json["playlists"] = OrderedJson{{"created", playlists.created}, {"merged", playlists.merged},
                                    {"referencesRemapped", playlists.referencesRemapped},
                                    {"missingReferences", playlists.missingReferences}};
    json["config"] = OrderedJson{{"applied", config.applied}, {"rejected", config.rejected}};
    json["extras"] = OrderedJson{{"favoritesImported", extras.favoritesImported},
                                 {"favoritesAlreadyPresent", extras.favoritesAlreadyPresent},
                                 {"recentImported", extras.recentImported}, {"recentSkipped", extras.recentSkipped},
                                 {"themeApplied", extras.themeApplied}, {"themeSkipped", extras.themeSkipped}};
    json["verification"] = OrderedJson{{"status", verificationStatus},
                                       {"checked", verificationChecked},
                                       {"failed", verificationFailed}};
    OrderedJson warningsJson = OrderedJson::array();
    for (const std::string &warning : warnings) {
        warningsJson.push_back(warning);
    }
    json["warnings"] = std::move(warningsJson);
    return json;
}

std::string legacySourceFingerprint(const LegacyInstall &install)
{
    const std::size_t hash = std::hash<std::string>{}(normalized(install.dataDirectory));
    return install.version + ":" + std::to_string(install.songCount) + ":" + std::to_string(hash);
}

LegacyVerification verifyImportedSheets(const std::filesystem::path &sourceData, const std::filesystem::path &destData,
                                        const std::vector<std::pair<std::string, std::string>> &expectedPayloads,
                                        const std::vector<std::string> &referencedPayloads)
{
    LegacyVerification verification;
    std::error_code error;
    for (const auto &[sourceFile, destFile] : expectedPayloads) {
        ++verification.checked;
        const std::filesystem::path destPath = destData / toWide(destFile);
        if (destFile.find_first_of("/\\") != std::string::npos ||
            !std::filesystem::is_regular_file(destPath, error)) {
            ++verification.failed;
            verification.failures.push_back("missing destination payload: " + destFile);
            continue;
        }
        if (!sourceFile.empty() && !sameFileBytes(sourceData / toWide(sourceFile), destPath)) {
            ++verification.failed;
            verification.failures.push_back("payload content mismatch: " + destFile);
        }
    }
    for (const std::string &referenced : referencedPayloads) {
        ++verification.checked;
        const std::filesystem::path destPath = destData / toWide(referenced);
        if (referenced.find_first_of("/\\") != std::string::npos ||
            !std::filesystem::is_regular_file(destPath, error) || !readFileBytes(destPath)) {
            ++verification.failed;
            verification.failures.push_back("unresolved payload reference: " + referenced);
        }
    }
    verification.verified = verification.failed == 0;
    return verification;
}

std::filesystem::path snapshotDestinationFiles(const std::filesystem::path &dataDirectory,
                                               const std::filesystem::path &configDirectory,
                                               const std::string &sourceFingerprint)
{
    const std::filesystem::path snapshotDir = dataDirectory / "migration-backup" / "pre-import";
    std::error_code error;
    const std::filesystem::path manifestPath = snapshotDir / "manifest.json";
    if (std::filesystem::is_regular_file(manifestPath, error)) {
        if (const auto manifest = readJson(manifestPath);
            manifest && manifest->value("sourceFingerprint", std::string()) == sourceFingerprint) {
            return snapshotDir;
        }
    }
    const std::array<std::pair<std::filesystem::path, const char *>, 4> files = {{
        {configDirectory / "config.json", "config.json"},
        {dataDirectory / "listSheet.json", "listSheet.json"},
        {dataDirectory / "playlists.json", "playlists.json"},
        {dataDirectory / "ui-state.json", "ui-state.json"},
    }};
    bool any = false;
    for (const auto &[source, name] : files) {
        if (std::filesystem::is_regular_file(source, error)) {
            any = true;
            break;
        }
    }
    if (!any) {
        return {};
    }
    std::filesystem::remove_all(snapshotDir, error);
    if (!ensureDir(snapshotDir)) {
        return {};
    }
    OrderedJson manifest = OrderedJson::object();
    manifest["version"] = 1;
    manifest["sourceFingerprint"] = sourceFingerprint;
    manifest["takenAt"] = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    OrderedJson copied = OrderedJson::array();
    for (const auto &[source, name] : files) {
        if (!std::filesystem::is_regular_file(source, error)) {
            continue;
        }
        if (std::filesystem::copy_file(source, snapshotDir / name,
                                       std::filesystem::copy_options::overwrite_existing, error)) {
            copied.push_back(name);
        }
    }
    if (copied.empty()) {
        return {};
    }
    manifest["files"] = std::move(copied);
    if (!atomicWrite(manifestPath, dumpJson(manifest, 4))) {
        return {};
    }
    return snapshotDir;
}

}  // namespace sky
