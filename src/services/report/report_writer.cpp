#include "services/report/report_writer.hpp"

#include <miniz.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>

namespace sky {

namespace {

Error writeError(const std::string &details)
{
    return Error{"REPORT_WRITE_FAILED", "The report could not be saved.", details};
}

bool alreadyCompressed(const std::string &name)
{
    const auto dot = name.find_last_of('.');
    if (dot == std::string::npos) {
        return false;
    }
    std::string extension = name.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == "jpg" || extension == "jpeg" || extension == "png" || extension == "zip";
}

std::optional<std::string> readAll(const std::filesystem::path &file)
{
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(input), {});
}

}  // namespace

std::string safeArchiveName(const std::string &name)
{
    std::string result;
    result.reserve(name.size());
    for (const char c : name) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20 || c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|') {
            result.push_back('_');
        } else {
            result.push_back(c);
        }
    }
    while (!result.empty() && (result.front() == '.' || result.front() == ' ')) {
        result.erase(result.begin());
    }
    return result.empty() ? std::string("file") : result;
}

Result<void> writeReportPackage(const std::filesystem::path &target, const ReportPackage &package)
{
    std::FILE *handle = nullptr;
    if (_wfopen_s(&handle, target.c_str(), L"wb") != 0 || !handle) {
        return writeError("cannot open " + target.filename().string());
    }
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_cfile(&zip, handle, 0)) {
        std::fclose(handle);
        return writeError("cannot start the archive");
    }
    std::set<std::string> used;
    bool ok = true;
    std::string failure;
    const auto add = [&](const std::string &name, const std::string &bytes) {
        if (!ok || !used.insert(name).second) {
            return;
        }
        const mz_uint level = alreadyCompressed(name) ? MZ_NO_COMPRESSION : MZ_DEFAULT_COMPRESSION;
        if (!mz_zip_writer_add_mem(&zip, name.c_str(), bytes.data(), bytes.size(), level)) {
            ok = false;
            failure = "cannot add " + name;
        }
    };

    add("report.json", dumpJson(package.manifest, 2));
    for (const auto &[name, bytes] : package.blobs) {
        add(name, bytes);
    }
    for (const auto &[name, file] : package.files) {
        if (auto bytes = readAll(file)) {
            add(name, *bytes);
        }
    }
    if (ok && !mz_zip_writer_finalize_archive(&zip)) {
        ok = false;
        failure = "cannot finish the archive";
    }
    mz_zip_writer_end(&zip);
    std::fclose(handle);
    if (!ok) {
        std::error_code error;
        std::filesystem::remove(target, error);
        return writeError(failure);
    }
    return {};
}

std::optional<std::string> readArchiveEntry(const std::filesystem::path &archive, const std::string &name)
{
    std::FILE *handle = nullptr;
    if (_wfopen_s(&handle, archive.c_str(), L"rb") != 0 || !handle) {
        return std::nullopt;
    }
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_cfile(&zip, handle, 0, 0)) {
        std::fclose(handle);
        return std::nullopt;
    }
    std::size_t size = 0;
    void *data = mz_zip_reader_extract_file_to_heap(&zip, name.c_str(), &size, 0);
    std::optional<std::string> result;
    if (data) {
        result = std::string(static_cast<const char *>(data), size);
        mz_free(data);
    }
    mz_zip_reader_end(&zip);
    std::fclose(handle);
    return result;
}

std::string readFileTail(const std::filesystem::path &file, std::size_t maxBytes)
{
    std::ifstream input(file, std::ios::binary | std::ios::ate);
    if (!input) {
        return {};
    }
    const std::streamoff size = input.tellg();
    const std::streamoff start = std::max<std::streamoff>(0, size - static_cast<std::streamoff>(maxBytes));
    input.seekg(start);
    std::string text(static_cast<std::size_t>(size - start), '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (start > 0) {
        if (const auto newline = text.find('\n'); newline != std::string::npos) {
            text.erase(0, newline + 1);
        }
    }
    return text;
}

std::optional<std::filesystem::path> latestCrashReport(const std::filesystem::path &directory)
{
    std::error_code error;
    std::optional<std::filesystem::path> latest;
    for (const auto &entry : std::filesystem::directory_iterator(directory, error)) {
        if (!entry.is_regular_file(error) || entry.path().extension() != kCrashReportExtension) {
            continue;
        }
        if (!latest || entry.path().filename() > latest->filename()) {
            latest = entry.path();
        }
    }
    return latest;
}

}  // namespace sky
