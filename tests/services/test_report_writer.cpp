#include <catch2/catch_test_macros.hpp>

#include <miniz.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "qt_test_support.hpp"
#include "services/report/report_writer.hpp"

using namespace sky;

namespace {

std::string readEntry(const std::filesystem::path &archive, const char *name)
{
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, archive.string().c_str(), 0)) {
        return "<no archive>";
    }
    std::size_t size = 0;
    void *data = mz_zip_reader_extract_file_to_heap(&zip, name, &size, 0);
    std::string text = data ? std::string(static_cast<const char *>(data), size) : std::string("<missing>");
    mz_free(data);
    mz_zip_reader_end(&zip);
    return text;
}

}  // namespace

TEST_CASE("a report package holds the manifest, text and files")
{
    TempDir dir;
    const auto frame = dir.path() / "frame.jpg";
    std::ofstream(frame, std::ios::binary) << "JPEGDATA";
    ReportPackage package;
    package.manifest["schema"] = kReportSchema;
    package.manifest["kind"] = "issue";
    package.blobs.emplace_back("log.txt", "line one\nline two\n");
    package.files.emplace_back("frames/000001.jpg", frame);
    package.files.emplace_back("env/missing.json", dir.path() / "does-not-exist.json");

    const auto target = dir.path() / "test.sapr";
    REQUIRE(writeReportPackage(target, package));
    const OrderedJson manifest = OrderedJson::parse(readEntry(target, "report.json"));
    CHECK(manifest["kind"] == "issue");
    CHECK(manifest["schema"] == kReportSchema);
    CHECK(readEntry(target, "log.txt") == "line one\nline two\n");
    CHECK(readEntry(target, "frames/000001.jpg") == "JPEGDATA");
    CHECK(readEntry(target, "env/missing.json") == "<missing>");
    CHECK(readArchiveEntry(target, "log.txt") == std::optional<std::string>("line one\nline two\n"));
    CHECK_FALSE(readArchiveEntry(target, "nothing.txt"));
}

TEST_CASE("the end of a log starts on a whole line")
{
    TempDir dir;
    const auto file = dir.path() / "app.log";
    std::ofstream(file, std::ios::binary) << "first line\nsecond line\nthird line\n";
    CHECK(readFileTail(file, 1000) == "first line\nsecond line\nthird line\n");
    CHECK(readFileTail(file, 16) == "third line\n");
    CHECK(readFileTail(dir.path() / "none.log", 100).empty());
}

TEST_CASE("archive names cannot escape the report or break Windows")
{
    CHECK(safeArchiveName("song.mid") == "song.mid");
    CHECK(safeArchiveName("../../evil.json") == "_.._evil.json");
    CHECK(safeArchiveName("a:b*c?.txt") == "a_b_c_.txt");
    CHECK(safeArchiveName("") == "file");
}

TEST_CASE("the newest crash report is found by its name")
{
    TempDir dir;
    CHECK_FALSE(latestCrashReport(dir.path()));
    std::ofstream(dir.path() / "20261118-155251-aaaa.sapcr") << "x";
    std::ofstream(dir.path() / "20261120-101010-bbbb.sapcr") << "x";
    std::ofstream(dir.path() / "20261121-000000-cccc.json") << "{}";
    const auto latest = latestCrashReport(dir.path());
    REQUIRE(latest);
    CHECK(latest->filename() == "20261120-101010-bbbb.sapcr");
}
