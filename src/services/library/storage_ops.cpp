#include "services/library/storage_ops.hpp"

#include <cmath>
#include <system_error>

#include "core/file_system.hpp"
#include "core/random.hpp"
#include "core/string_utils.hpp"
#include "core/text_file.hpp"
#include "core/translator.hpp"

namespace sky {

StorageFileOps StorageFileOps::native() {
    StorageFileOps ops;
    ops.ensureDir = [](const std::filesystem::path &dir) {
        return sky::ensureDir(dir);
    };
    ops.exists = [](const std::filesystem::path &path) {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    };
    ops.isRegularFile = [](const std::filesystem::path &path) {
        std::error_code ec;
        return std::filesystem::is_regular_file(path, ec);
    };
    ops.readText = [](const std::filesystem::path &file) {
        return sky::readTextFile(file);
    };
    ops.readBytes = [](const std::filesystem::path &file) {
        return sky::readFileBytes(file);
    };
    ops.atomicWrite = [](const std::filesystem::path &file, std::string_view bytes) {
        return sky::atomicWrite(file, bytes);
    };
    ops.copyFile = [](const std::filesystem::path &from,
                      const std::filesystem::path &to) -> Result<void> {
        std::error_code ec;
        if (!std::filesystem::copy_file(from, to, ec)) {
            return Error{
                "COPY_FAILED",
                fromQString(
                    tr("Unable to access %1.").arg(toQString(fromWide(to.filename().wstring())))),
                fromWide(from.wstring()) + " -> " + fromWide(to.wstring()) + ": " + ec.message()};
        }
        return {};
    };
    ops.removeFile = [](const std::filesystem::path &file) {
        return sky::safeDelete(file);
    };
    ops.nextSalt = []() {
        return static_cast<std::int64_t>(std::floor(randomUnit() * 10000)) + 1;
    };
    return ops;
}

StorageFileOps StorageFileOps::withDefaults(StorageFileOps ops) {
    StorageFileOps defaults = native();
    if (!ops.ensureDir) {
        ops.ensureDir = std::move(defaults.ensureDir);
    }
    if (!ops.exists) {
        ops.exists = std::move(defaults.exists);
    }
    if (!ops.isRegularFile) {
        ops.isRegularFile = std::move(defaults.isRegularFile);
    }
    if (!ops.readText) {
        ops.readText = std::move(defaults.readText);
    }
    if (!ops.readBytes) {
        ops.readBytes = std::move(defaults.readBytes);
    }
    if (!ops.atomicWrite) {
        ops.atomicWrite = std::move(defaults.atomicWrite);
    }
    if (!ops.copyFile) {
        ops.copyFile = std::move(defaults.copyFile);
    }
    if (!ops.removeFile) {
        ops.removeFile = std::move(defaults.removeFile);
    }
    if (!ops.nextSalt) {
        ops.nextSalt = std::move(defaults.nextSalt);
    }
    return ops;
}

}  // namespace sky
