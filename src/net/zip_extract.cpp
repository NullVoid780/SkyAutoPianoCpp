#include "net/zip_extract.hpp"

#include <miniz.h>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "core/translator.hpp"
#include "core/file_system.hpp"
#include "core/string_utils.hpp"

namespace sky {

namespace {

Error zipError(const std::string &details)
{
    return Error{"ZIP_FAILED", trUtf8("Unable to complete update."), details};
}

std::size_t writeChunk(void *opaque, mz_uint64, const void *buffer, std::size_t size)
{
    auto *stream = static_cast<std::ofstream *>(opaque);
    stream->write(static_cast<const char *>(buffer), static_cast<std::streamsize>(size));
    return stream->good() ? size : 0;
}

}  // namespace

Result<std::vector<std::filesystem::path>> extractZip(const std::filesystem::path &archive,
                                                      const std::filesystem::path &destination)
{
    auto bytes = readFileBytes(archive);
    if (!bytes) {
        return zipError(bytes.error().details);
    }
    auto zip = std::make_unique<mz_zip_archive>();
    mz_zip_zero_struct(zip.get());
    if (!mz_zip_reader_init_mem(zip.get(), bytes.value().data(), bytes.value().size(), 0)) {
        return zipError("not a zip archive");
    }
    const auto close = [&zip] { mz_zip_reader_end(zip.get()); };
    std::error_code error;
    const std::filesystem::path root = std::filesystem::weakly_canonical(destination, error);
    std::vector<std::filesystem::path> written;
    const mz_uint count = mz_zip_reader_get_num_files(zip.get());
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(zip.get(), i, &stat)) {
            close();
            return zipError("unreadable entry");
        }
        const std::filesystem::path relative = std::filesystem::path(reinterpret_cast<const char8_t *>(stat.m_filename));
        if (relative.is_absolute() || relative.has_root_name()) {
            close();
            return zipError("absolute path in archive");
        }
        const std::filesystem::path target = std::filesystem::weakly_canonical(root / relative, error);
        if (!isInsideDirectory(target, root)) {
            close();
            return zipError("path escapes the destination");
        }
        if (mz_zip_reader_is_file_a_directory(zip.get(), i)) {
            std::filesystem::create_directories(target, error);
            continue;
        }
        std::filesystem::create_directories(target.parent_path(), error);
        std::ofstream stream(target, std::ios::binary | std::ios::trunc);
        if (!stream || !mz_zip_reader_extract_to_callback(zip.get(), i, writeChunk, &stream, 0)) {
            close();
            return zipError("failed to write " + target.filename().string());
        }
        written.push_back(target);
    }
    close();
    return written;
}

}  // namespace sky
