#include <catch2/catch_test_macros.hpp>

#include "services/store/store_mapping.hpp"
#include "ui/main_window/store_pagination.hpp"

using namespace sky;

namespace {

constexpr const char *kId = "0f8fad5b-d9cb-469f-a165-70867728950e";

OrderedJson summary(const std::string &extra = "")
{
    return OrderedJson::parse(std::string(R"({"id":")") + kId + R"(")" + extra + "}");
}

StorePlayerPayload playerPayload(const std::string &notes, int bitsPerPage = 15)
{
    const std::string text = std::string(R"({"sheet":{"id":")") + kId +
                             R"(","title":" Song "},"player":{"bitsPerPage":)" + std::to_string(bitsPerPage) +
                             R"(},"runtime":{"notes":)" + notes + "}}";
    return mapStorePlayerPayload(OrderedJson::parse(text)).value();
}

}  // namespace

TEST_CASE("validates sheet ids like the original UUID pattern")
{
    CHECK(isStoreSheetId(kId));
    CHECK(isStoreSheetId("0F8FAD5B-D9CB-469F-A165-70867728950E"));
    CHECK_FALSE(isStoreSheetId("0f8fad5b-d9cb-669f-a165-70867728950e"));
    CHECK_FALSE(isStoreSheetId("0f8fad5b-d9cb-469f-c165-70867728950e"));
    CHECK_FALSE(isStoreSheetId("not-a-uuid"));
}

TEST_CASE("clampInteger falls back for non-finite and out-of-range values")
{
    CHECK(clampInteger(OrderedJson("42.9"), 0, 100, 7) == 42);
    CHECK(clampInteger(OrderedJson(OrderedJson::value_t::discarded), 0, 100, 7) == 7);
    CHECK(clampInteger(OrderedJson(), 0, 100, 7) == 0);
    CHECK(mapStorePage(OrderedJson::parse(R"({"items":[]})"), 20, 40).value().offset == 40);
    CHECK(clampInteger(OrderedJson(101), 0, 100, 7) == 7);
    CHECK(clampInteger(OrderedJson("x"), 0, 100, 7) == 7);
}

TEST_CASE("maps catalogue summaries with camelCase or snake_case fields")
{
    auto item = mapStoreSummary(summary(
        R"(,"title":"  Hello ","artist_name":"Artist","bits_per_page":15,"pitchLevel":3,"cover_url":"/covers/a.png","tags":[" a ","",1],"is_downloadable":false,"creator":{"handle":"h"})"));
    REQUIRE(item.ok());
    const StoreItem &mapped = item.value();
    CHECK(mapped.sourceType == "user");
    CHECK(mapped.catalogKey == std::string("user:") + kId);
    CHECK(mapped.title == "Hello");
    CHECK(mapped.author == "Artist");
    CHECK(mapped.transcribedBy == "Unknown");
    CHECK(mapped.bitsPerPage == 15);
    CHECK(mapped.pitchLevel == 3);
    CHECK(mapped.coverUrl == "https://api.skysheet.store/covers/a.png");
    CHECK(mapped.tags == std::vector<std::string>{"a"});
    CHECK_FALSE(mapped.isDownloadable);
    REQUIRE(mapped.creator.has_value());
    CHECK(mapped.creator->displayName == "Unknown");
    CHECK(mapped.currency == "USD");
    CHECK(mapped.accessMode == "downloadable");
    CHECK(mapped.sourceUrl == std::string("https://skysheet.store/sheets/") + kId);
}

TEST_CASE("cover URLs outside the API host are dropped")
{
    CHECK(mapStoreSummary(summary(R"(,"coverUrl":"https://evil.example/a.png")")).value().coverUrl.empty());
    CHECK(mapStoreSummary(summary(R"(,"coverUrl":"http://api.skysheet.store/a.png")")).value().coverUrl.empty());
    CHECK(mapStoreSummary(summary(R"(,"coverUrl":"https://api.skysheet.store/a.png")")).value().coverUrl ==
          "https://api.skysheet.store/a.png");
}

TEST_CASE("archive items link to the archive page")
{
    auto item = mapStoreSummary(summary(R"(,"source_type":"archive")"));
    REQUIRE(item.ok());
    CHECK(item.value().sourceUrl == std::string("https://skysheet.store/sheets/archive/") + kId);
    CHECK(storeAvailability(item.value()).canPlay);
}

TEST_CASE("an invalid id fails the whole page")
{
    auto page = mapStorePage(OrderedJson::parse(R"({"items":[{"id":"bad"}]})"), 20, 0);
    REQUIRE_FALSE(page.ok());
    CHECK(page.error().userMessage == "Sky Sheet Store returned an invalid sheet id.");
    auto missing = mapStorePage(OrderedJson::parse(R"({"total":3})"), 20, 0);
    CHECK(missing.error().userMessage == "Sky Sheet Store returned an invalid sheet list.");
}

TEST_CASE("page numbers fall back to the request values")
{
    auto page = mapStorePage(OrderedJson::parse(R"({"items":[],"limit":500,"offset":-1,"total":"12"})"), 20, 40);
    REQUIRE(page.ok());
    CHECK(page.value().limit == 20);
    CHECK(page.value().offset == 40);
    CHECK(page.value().total == 12);
}

TEST_CASE("availability rules decide play and save")
{
    StoreItem item;
    item.sourceType = "user";
    item.accessMode = "web_only";
    CHECK(storeAvailability(item).message == "Web only");
    item.accessMode = "downloadable";
    item.isDownloadable = false;
    CHECK(storeAvailability(item).message == "Not downloadable");
    item.isDownloadable = true;
    item.priceAmount = 3;
    CHECK(storeAvailability(item).message == "Purchase required");
    item.priceAmount = 0;
    item.viewerActionState = "login_to_open";
    CHECK(storeAvailability(item).message == "Login required");
    CHECK_FALSE(storeAvailability(item).canSave);
    item.viewerActionState.clear();
    CHECK(storeAvailability(item).canPlay);
}

TEST_CASE("incomplete player payloads are rejected")
{
    auto payload = mapStorePlayerPayload(OrderedJson::parse(R"({"sheet":{},"player":{}})"));
    REQUIRE_FALSE(payload.ok());
    CHECK(payload.error().userMessage == "Sky Sheet Store returned an incomplete player payload.");
}

TEST_CASE("player payloads become a playable sheet")
{
    StoreItem item;
    item.author = "Item Author";
    const auto payload = playerPayload(R"([{"timeMs":0,"keys":["1Key0","1Key0","Key5"]},{"timeMs":500,"keys":[]},{"timeMs":500,"keys":["1Key14"]}])");
    auto prepared = prepareStoreSheet(payload, item);
    REQUIRE(prepared.ok());
    const OrderedJson &sheet = prepared.value().normalizedSheet;
    CHECK(sheet["name"] == "Song");
    CHECK(sheet["author"] == "Item Author");
    CHECK(sheet["transcribedBy"] == "Unknown");
    CHECK(sheet["bpm"] == 120);
    CHECK(sheet["songNotes"].size() == 3);
    CHECK(dumpJson(prepared.value().keyMap.toJson()) == R"({"0":["h","y"],"500":["/"],"1000":[]})");
    CHECK_FALSE(prepared.value().hasHoldNotes);
}

TEST_CASE("invalid runtime notes make the download invalid")
{
    StoreItem item;
    const auto check = [&](const std::string &notes, int bits = 15) {
        auto prepared = prepareStoreSheet(playerPayload(notes, bits), item);
        REQUIRE_FALSE(prepared.ok());
        CHECK(prepared.error().userMessage == "The downloaded sheet is invalid.");
    };
    check("[]");
    check(R"([{"timeMs":500,"keys":["1Key0"]},{"timeMs":100,"keys":["1Key0"]}])");
    check(R"([{"timeMs":0,"keys":["1Key12"]}])", 8);
    check(R"([{"timeMs":0,"keys":["Bad"]}])");
    check(R"([{"timeMs":0,"keys":[]}])");
}

TEST_CASE("store pagination shows a five-page window around the current page")
{
    CHECK(StorePagination::pageWindow(1, 3) == std::pair{1, 3});
    CHECK(StorePagination::pageWindow(1, 10) == std::pair{1, 5});
    CHECK(StorePagination::pageWindow(5, 10) == std::pair{3, 7});
    CHECK(StorePagination::pageWindow(10, 10) == std::pair{6, 10});
    CHECK(StorePagination::pageWindow(12, 10) == std::pair{6, 10});
}