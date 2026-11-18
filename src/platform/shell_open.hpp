#pragma once

#include <filesystem>

class QUrl;

namespace sky {

bool openExternalUrl(const QUrl &url);
bool openLocalFolder(const std::filesystem::path &folder);

}  // namespace sky
