#pragma once

#include <filesystem>
#include <optional>
#include <set>
#include <string>
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

struct LegacyImportPlan {
    LegacyInstall install;
    std::vector<OrderedJson> rows;
    std::vector<OrderedJson> allRows;
    int alreadyPresent = 0;
    LegacyExtras extras;
};

std::string legacySongKey(const OrderedJson &row);
std::optional<LegacyInstall> inspectLegacyFolder(const std::filesystem::path &folder);
std::vector<LegacyInstall> findLegacyInstalls(const std::filesystem::path &ownDataDirectory,
                                              const std::vector<std::filesystem::path> &searchRoots,
                                              bool useRegistryHints = true);
std::vector<std::filesystem::path> defaultLegacySearchRoots();
Result<LegacyImportPlan> planLegacyImport(const LegacyInstall &install, const std::set<std::string> &existingKeys,
                                          const std::filesystem::path &localStorageDirectory = {});
LegacyExtras readLegacyExtras(const LegacyInstall &install, const std::filesystem::path &localStorageDirectory);

}  // namespace sky
