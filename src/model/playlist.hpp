#pragma once

#include <string>
#include <vector>

namespace sky {

inline constexpr std::size_t kPlaylistNameMaxLength = 80;

struct Playlist {
    std::string id;
    std::string name;
    std::vector<std::string> sheetKeys;

    bool operator==(const Playlist &) const = default;
};

}  // namespace sky
