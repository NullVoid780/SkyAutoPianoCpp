#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/json.hpp"
#include "core/result.hpp"

namespace sky {

struct LegacyInstall {
    std::filesystem::path root;
    std::filesystem::path dataDirectory;
    std::string version;
    int songCount = 0;
};

struct LegacyExtras {
    std::optional<OrderedJson> config;
    OrderedJson playlists = OrderedJson::array();
    std::vector<std::string> favorites;
    std::vector<std::string> recentPlays;
    std::optional<std::string> theme;

    bool empty() const
    {
        return !config && playlists.empty() && favorites.empty() && recentPlays.empty() && !theme;
    }
};

// A source row whose (name, author, transcribedBy) key already exists in the
// C++ library but whose payload bytes differ from the destination payload.
// Such a row is NOT "already present": it is never copied over the existing
// payload and never reported as verified; it is surfaced as a conflict.
struct LegacySheetConflict {
    OrderedJson row;
    std::string sourceFile;
    std::string destinationFile;
};

struct LegacyImportPlan {
    LegacyInstall install;
    std::vector<OrderedJson> rows;
    std::vector<OrderedJson> allRows;
    int alreadyPresent = 0;
    std::vector<LegacySheetConflict> conflicts;
    int invalidRows = 0;
    int missingPayloads = 0;
    LegacyExtras extras;
};

// Structured outcome of one import run. Counts are measured, never assumed:
// a category that could not be checked stays visible instead of being folded
// into a generic success.
struct LegacyImportResult {
    struct Sheets {
        int planned = 0;
        int copied = 0;
        int alreadyPresent = 0;
        int conflicts = 0;
        int failed = 0;
        int validated = 0;
    } sheets;
    struct Playlists {
        int created = 0;
        int merged = 0;
        int referencesRemapped = 0;
        int missingReferences = 0;
    } playlists;
    struct Config {
        bool applied = false;
        bool rejected = false;
    } config;
    struct Extras {
        int favoritesImported = 0;
        int favoritesAlreadyPresent = 0;
        bool recentImported = false;
        bool recentSkipped = false;
        bool themeApplied = false;
        bool themeSkipped = false;
    } extras;
    // "verified" only when every accepted artifact passed verification;
    // anything partial is "needs_review".
    std::string verificationStatus = "needs_review";
    int verificationChecked = 0;
    int verificationFailed = 0;
    std::vector<std::string> warnings;
    std::string sourceFingerprint;
    // The importer only ever reads from the legacy installation.
    std::string sourceAccess = "read-only";

    OrderedJson toJson() const;
};

struct LegacyVerification {
    bool verified = false;
    int checked = 0;
    int failed = 0;
    std::vector<std::string> failures;
};

// Post-import check: every expected (source file -> destination file) payload
// must exist in the destination and match the source bytes, and every
// referenced destination payload file must exist and be readable.
LegacyVerification verifyImportedSheets(const std::filesystem::path &sourceData, const std::filesystem::path &destData,
                                        const std::vector<std::pair<std::string, std::string>> &expectedPayloads,
                                        const std::vector<std::string> &referencedPayloads);

// Stable identity of a legacy source for migration state. Deliberately not
// the absolute path: a hash of the canonical data directory plus the
// package version and song count, so state files do not leak user paths.
std::string legacySourceFingerprint(const LegacyInstall &install);

// Scoped snapshot of pre-existing destination files (config.json,
// listSheet.json, playlists.json, ui-state.json) taken before an import
// mutates them. Lives in <dataDirectory>/migration-backup/pre-import, a
// single retained set: an existing snapshot for the same source fingerprint
// is kept (it holds the oldest, pre-first-attempt state); a snapshot for a
// different source is replaced. Returns the snapshot directory, or an empty
// path when there was nothing to snapshot or the copy failed.
std::filesystem::path snapshotDestinationFiles(const std::filesystem::path &dataDirectory,
                                               const std::filesystem::path &configDirectory,
                                               const std::string &sourceFingerprint);

std::string legacySongKey(const OrderedJson &row);
std::optional<LegacyInstall> inspectLegacyFolder(const std::filesystem::path &folder);
std::vector<LegacyInstall> findLegacyInstalls(const std::filesystem::path &ownDataDirectory,
                                              const std::vector<std::filesystem::path> &searchRoots,
                                              bool useRegistryHints = true);
std::vector<std::filesystem::path> defaultLegacySearchRoots();
// existingPayloads maps a destination song key (legacySongKey) to the payload
// file name the C++ library already stores for it. When both it and
// destDataDirectory are provided, "already present" is decided by comparing
// payload bytes, and same-key/different-bytes rows become conflicts instead.
Result<LegacyImportPlan> planLegacyImport(const LegacyInstall &install, const std::set<std::string> &existingKeys,
                                          const std::filesystem::path &localStorageDirectory = {},
                                          const std::filesystem::path &destDataDirectory = {},
                                          const std::map<std::string, std::string> &existingPayloads = {});
LegacyExtras readLegacyExtras(const LegacyInstall &install, const std::filesystem::path &localStorageDirectory);

}  // namespace sky
