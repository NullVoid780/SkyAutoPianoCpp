#pragma once

#include <filesystem>

#include "core/result.hpp"

namespace sky {

class Paths {
public:
    static std::filesystem::path appDirectory();
    static std::filesystem::path configDirectory();
    static std::filesystem::path dataDirectory();
    static std::filesystem::path logDirectory();
    static std::filesystem::path logFile();
    static Result<void> ensureDirectories();
};

}  // namespace sky
