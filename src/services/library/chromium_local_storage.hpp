#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace sky {

std::optional<std::string> snappyDecompress(std::string_view input);
std::map<std::string, std::string> readChromiumLocalStorage(const std::filesystem::path &levelDbDirectory,
                                                            std::string_view origin);
std::filesystem::path electronLocalStorageDirectory();

}  // namespace sky
