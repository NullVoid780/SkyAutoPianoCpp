#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"

namespace sky {

struct StorageFileOps {
    std::function<Result<void>(const std::filesystem::path &)> ensureDir;
    std::function<bool(const std::filesystem::path &)> exists;
    std::function<bool(const std::filesystem::path &)> isRegularFile;
    std::function<Result<std::string>(const std::filesystem::path &)> readText;
    std::function<Result<std::vector<std::byte>>(const std::filesystem::path &)> readBytes;
    std::function<Result<void>(const std::filesystem::path &, std::string_view)> atomicWrite;
    std::function<Result<void>(const std::filesystem::path &, const std::filesystem::path &)>
        copyFile;
    std::function<Result<void>(const std::filesystem::path &)> removeFile;
    std::function<std::int64_t()> nextSalt;

    static StorageFileOps native();
    static StorageFileOps withDefaults(StorageFileOps ops);
};

}  // namespace sky
