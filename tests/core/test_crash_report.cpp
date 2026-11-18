#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>

#include "crash/breadcrumbs.hpp"
#include "crash/crash_report.hpp"
#include "crash/pending_reports.hpp"

using namespace sky;

TEST_CASE("access violations are described with the operation and address")
{
    EXCEPTION_RECORD record{};
    record.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
    record.NumberParameters = 2;
    record.ExceptionInformation[0] = 0;
    record.ExceptionInformation[1] = 0x18;
    CHECK(describeException(record) == "EXCEPTION_ACCESS_VIOLATION reading 0x0000000000000018");
    record.ExceptionInformation[0] = 1;
    CHECK(describeException(record) == "EXCEPTION_ACCESS_VIOLATION writing 0x0000000000000018");
    CHECK(exceptionCodeName(kTerminateCode) == "CPP_TERMINATE");
    CHECK(exceptionCodeName(0x12345678) == "UNKNOWN_EXCEPTION");
}

TEST_CASE("crash reports follow schema 1")
{
    CrashReportInput input;
    input.appVersion = "2.0.0";
    input.reason = CrashReason::SehException;
    input.code = EXCEPTION_ACCESS_VIOLATION;
    input.description = "EXCEPTION_ACCESS_VIOLATION reading 0x0000000000000018";
    input.address = 0x7FF6A21C4E31;
    input.module = "SkyAutoPiano.exe";
    input.moduleOffset = 0x1C4E31;
    input.threadId = 11240;
    input.threadName = "playback";
    input.stack = {"SkyAutoPiano.exe!sky::StrictPlayer::playback+0x12a"};
    input.modules = {CrashModule{"SkyAutoPiano.exe", 0x7FF6A2000000, 7340032, "2.0.0.0"}};
    CrashState state{};
    state.playbackMode = 1;
    state.playing = 1;
    state.speed = 1.2;
    state.theme = 1;
    state.librarySize = 142;
    input.state = state;
    input.breadcrumbs = {BreadcrumbEntry{183991, "playback", "start mode=strict"}};
    input.dump = CrashDump{"x.dmp", 2841600, "abc"};

    const auto report = buildCrashReport(input);
    CHECK(report["schema"] == 1);
    CHECK(report["app"]["version"] == "2.0.0");
    CHECK(report["crash"]["reason"] == "seh_exception");
    CHECK(report["crash"]["code"] == "0xC0000005");
    CHECK(report["crash"]["address"] == "0x00007FF6A21C4E31");
    CHECK(report["crash"]["module_offset"] == "0x1C4E31");
    CHECK(report["crash"]["thread_name"] == "playback");
    CHECK(report["modules"][0]["base"] == "0x7FF6A2000000");
    CHECK(report["state"]["playback_mode"] == "strict");
    CHECK(report["state"]["theme"] == "dark");
    CHECK(report["state"]["library_size"] == 142);
    CHECK(report["breadcrumbs"][0]["c"] == "playback");
    CHECK(report["dump"]["bytes"] == 2841600);
}

TEST_CASE("unexpected exits carry no exception details and no dump")
{
    CrashReportInput input;
    input.unexpectedExit = true;
    const auto report = buildCrashReport(input);
    CHECK(report["crash"]["reason"] == "unexpected_exit");
    CHECK_FALSE(report["crash"].contains("code"));
    CHECK(report["dump"].is_null());
}

TEST_CASE("the breadcrumb ring keeps the newest 128 records in order")
{
    auto context = std::make_unique<CrashContext>();
    ZeroMemory(context.get(), sizeof(CrashContext));
    for (int i = 0; i < 200; ++i) {
        writeBreadcrumb(*context, "test", std::to_string(i).c_str());
    }
    const auto entries = readBreadcrumbs(*context);
    REQUIRE(entries.size() == 128);
    CHECK(entries.front().message == "72");
    CHECK(entries.back().message == "199");
    writeBreadcrumb(*context, "a-very-long-category-name", std::string(300, 'x').c_str());
    const auto last = readBreadcrumbs(*context).back();
    CHECK(last.category.size() == kBreadcrumbCategoryBytes - 1);
    CHECK(last.message.size() == kBreadcrumbMessageBytes - 1);
}

TEST_CASE("pruning keeps the newest reports and drops oversized dumps")
{
    const auto dir = std::filesystem::temp_directory_path() / "sky_crash_prune_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const auto touch = [&](const std::string &name, std::size_t bytes) {
        std::ofstream(dir / name, std::ios::binary) << std::string(bytes, 'x');
    };
    for (int i = 0; i < 12; ++i) {
        const std::string id = "20261116-1000" + std::string(i < 10 ? "0" : "") + std::to_string(i) + "-aa";
        touch(id + ".json", 10);
        touch(id + ".dmp", i == 11 ? 64 : 10);
    }
    pruneCrashReports(dir, 10, 32);
    CHECK_FALSE(std::filesystem::exists(dir / "20261116-100000-aa.json"));
    CHECK_FALSE(std::filesystem::exists(dir / "20261116-100001-aa.json"));
    CHECK(std::filesystem::exists(dir / "20261116-100002-aa.json"));
    CHECK(std::filesystem::exists(dir / "20261116-100011-aa.json"));
    CHECK_FALSE(std::filesystem::exists(dir / "20261116-100011-aa.dmp"));
    std::filesystem::remove_all(dir);
}
