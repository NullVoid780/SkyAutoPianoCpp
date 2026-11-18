#include <catch2/catch_test_macros.hpp>

#include "codec/encrypted_notes.hpp"
#include "codec/sheet_codec.hpp"
#include "test_helpers.hpp"

using namespace sky;

namespace {

constexpr const char *kEncryptedNotes =
    "[75,89,-22,98,43,90,126,-21,43,24,14,22,77,56,82,57,68,19,4,93,64,123,69,-31,100,97]";

}  // namespace

TEST_CASE("decodes the masked note array")
{
    auto decoded = decodeEncryptedNotes(OrderedJson::parse(kEncryptedNotes));
    REQUIRE(decoded.ok());
    CHECK(dumpJson(decoded.value()) == R"([{"time":0,"key":"1Key0"}])");
}

TEST_CASE("decrypts encrypted numeric notes during import")
{
    const std::string source = std::string(R"([{"name":"Fixture","author":"Tester","bpm":120,"pitchLevel":0,"isEncrypted":true,"songNotes":)") +
                               kEncryptedNotes + "}]";
    const auto bytes = bytesOf(source);
    auto result = parseSheet(bytes, "encrypted.json");
    REQUIRE(result.ok());
    CHECK_FALSE(result.value().sheet.isEncrypted);
    CHECK(result.value().sheet.songNotes == std::vector<SongNote>{{0, "1Key0", 0}});
}

TEST_CASE("numeric first notes trigger decryption even without the flag")
{
    OrderedJson sheet = OrderedJson::parse(R"({"songNotes":[1,2]})");
    CHECK(hasEncryptedNotes(sheet));
    CHECK_FALSE(hasEncryptedNotes(OrderedJson::parse(R"({"songNotes":[]})")));
    CHECK_FALSE(hasEncryptedNotes(OrderedJson::parse(R"({"isEncrypted":"true"})")));
}

TEST_CASE("undecodable encrypted notes report DECRYPT_FAILED")
{
    const auto bytes = bytesOf(R"([{"name":"X","isEncrypted":true,"songNotes":[1,2,3]}])");
    auto result = parseSheet(bytes, "bad.json");
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == "DECRYPT_FAILED");
    CHECK(result.error().userMessage ==
          "The sheet is encrypted but could not be decrypted. The file may be corrupted.");

    const auto missing = bytesOf(R"([{"name":"X","isEncrypted":true}])");
    auto noNotes = parseSheet(missing, "bad.json");
    REQUIRE_FALSE(noNotes.ok());
    CHECK(noNotes.error().code == "DECRYPT_FAILED");
}
