#include "services/library/legacy_import.hpp"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <system_error>

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
                                          const std::filesystem::path &localStorageDirectory)
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
            continue;
        }
        plan.allRows.push_back(row);
        const std::string key = legacySongKey(row);
        if (existingKeys.contains(key) || !planned.insert(key).second) {
            ++plan.alreadyPresent;
            continue;
        }
        plan.rows.push_back(row);
    }
    plan.extras = readLegacyExtras(install, localStorageDirectory);
    return plan;
}

}  // namespace sky
