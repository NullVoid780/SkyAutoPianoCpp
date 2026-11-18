#include "core/file_system.hpp"

#include <windows.h>

#include <fstream>
#include <system_error>

#include "core/string_utils.hpp"

namespace sky {

namespace fs = std::filesystem;

namespace {

Error fileError(std::string code, const fs::path &path, const std::error_code &ec)
{
    return Error{std::move(code), "Unable to access " + fromWide(path.filename().wstring()) + ".",
                 fromWide(path.wstring()) + ": " + ec.message()};
}

}  // namespace

Result<void> ensureDir(const fs::path &dir)
{
    std::error_code ec;
    if (fs::is_directory(dir, ec)) {
        return {};
    }
    fs::create_directories(dir, ec);
    if (ec) {
        return fileError("ENSURE_DIR_FAILED", dir, ec);
    }
    return {};
}

Result<void> copyDir(const fs::path &from, const fs::path &to)
{
    std::error_code ec;
    fs::create_directories(to, ec);
    if (ec) {
        return fileError("COPY_DIR_FAILED", to, ec);
    }
    fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) {
        return fileError("COPY_DIR_FAILED", from, ec);
    }
    return {};
}

Result<void> removeDir(const fs::path &dir)
{
    std::error_code ec;
    fs::remove_all(dir, ec);
    if (ec) {
        return fileError("REMOVE_DIR_FAILED", dir, ec);
    }
    return {};
}

Result<void> safeDelete(const fs::path &file)
{
    std::error_code ec;
    if (!fs::exists(file, ec)) {
        return {};
    }
    fs::remove(file, ec);
    if (ec) {
        return fileError("DELETE_FAILED", file, ec);
    }
    return {};
}

Result<void> atomicWrite(const fs::path &file, std::string_view bytes)
{
    fs::path temp = file;
    temp += L".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return fileError("WRITE_FAILED", file, std::make_error_code(std::errc::permission_denied));
        }
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) {
            return fileError("WRITE_FAILED", file, std::make_error_code(std::errc::io_error));
        }
    }
    if (!MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const std::error_code ec(static_cast<int>(GetLastError()), std::system_category());
        std::error_code ignored;
        fs::remove(temp, ignored);
        return fileError("WRITE_FAILED", file, ec);
    }
    return {};
}

Result<std::vector<std::byte>> readFileBytes(const fs::path &file)
{
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in) {
        return fileError("READ_FAILED", file, std::make_error_code(std::errc::no_such_file_or_directory));
    }
    const std::streamsize size = in.tellg();
    std::vector<std::byte> bytes(static_cast<std::size_t>(size < 0 ? 0 : size));
    in.seekg(0);
    if (!bytes.empty() && !in.read(reinterpret_cast<char *>(bytes.data()), size)) {
        return fileError("READ_FAILED", file, std::make_error_code(std::errc::io_error));
    }
    return bytes;
}

bool isInsideDirectory(const fs::path &candidate, const fs::path &root)
{
    std::error_code ec;
    const fs::path full = fs::weakly_canonical(candidate, ec);
    if (ec) {
        return false;
    }
    const fs::path base = fs::weakly_canonical(root, ec);
    if (ec) {
        return false;
    }
    const fs::path relative = full.lexically_relative(base);
    return !relative.empty() && *relative.begin() != L".." && relative != L".";
}

}  // namespace sky
