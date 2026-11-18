#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <fstream>
#include <string>

#include "qt_test_support.hpp"
#include "services/library/chromium_local_storage.hpp"

using namespace sky;

namespace {

std::string varint(std::uint64_t value)
{
    std::string out;
    while (value >= 0x80) {
        out.push_back(static_cast<char>((value & 0x7f) | 0x80));
        value >>= 7;
    }
    out.push_back(static_cast<char>(value));
    return out;
}

std::string fixed(std::uint64_t value, int width)
{
    std::string out;
    for (int i = 0; i < width; ++i) {
        out.push_back(static_cast<char>((value >> (8 * i)) & 0xff));
    }
    return out;
}

std::string storageKey(const std::string &name)
{
    return std::string("_file://") + std::string(1, '\0') + "\x01" + name;
}

std::string latin1Value(const std::string &text)
{
    return std::string("\x01") + text;
}

std::string writeBatch(std::uint64_t sequence, const std::vector<std::pair<std::string, std::optional<std::string>>> &ops)
{
    std::string batch = fixed(sequence, 8) + fixed(ops.size(), 4);
    for (const auto &[key, value] : ops) {
        batch.push_back(value ? '\x01' : '\x00');
        batch += varint(key.size()) + key;
        if (value) {
            batch += varint(value->size()) + *value;
        }
    }
    return batch;
}

std::string logRecord(const std::string &payload)
{
    return std::string(4, '\0') + fixed(payload.size(), 2) + std::string(1, '\x01') + payload;
}

std::string block(const std::vector<std::pair<std::string, std::string>> &entries)
{
    std::string out;
    for (const auto &[key, value] : entries) {
        out += varint(0) + varint(key.size()) + varint(value.size()) + key + value;
    }
    return out + fixed(0, 4) + fixed(1, 4);
}

std::string snappyLiteral(const std::string &raw)
{
    return varint(raw.size()) + std::string(1, static_cast<char>((raw.size() - 1) << 2)) + raw;
}

void writeBytes(const std::filesystem::path &file, const std::string &bytes)
{
    std::ofstream(file, std::ios::binary) << bytes;
}

}  // namespace

TEST_CASE("snappy decompresses literals and back-references")
{
    const std::string literal = snappyLiteral("hello");
    CHECK(snappyDecompress(literal) == "hello");
    const std::string copy = varint(10) + std::string(1, static_cast<char>(4 << 2)) + "abcde" +
                             std::string(1, static_cast<char>(((5 - 4) << 2) | 1)) + std::string(1, '\x05');
    CHECK(snappyDecompress(copy) == "abcdeabcde");
    CHECK_FALSE(snappyDecompress(varint(10) + std::string(1, '\x09') + std::string(1, '\x05')));
}

TEST_CASE("local storage values are read from the write-ahead log, newest first")
{
    TempDir dir;
    std::string log = logRecord(writeBatch(1, {{storageKey("favorites"), latin1Value(R"(["Old"])")},
                                               {storageKey("theme"), latin1Value("light")},
                                               {storageKey("gone"), latin1Value("x")}}));
    log += logRecord(writeBatch(10, {{storageKey("favorites"), latin1Value(R"(["Rubia","Lullaby"])")},
                                     {storageKey("gone"), std::nullopt}}));
    writeBytes(dir.path() / "000003.log", log);

    const auto values = readChromiumLocalStorage(dir.path(), "file://");
    CHECK(values.at("favorites") == R"(["Rubia","Lullaby"])");
    CHECK(values.at("theme") == "light");
    CHECK_FALSE(values.contains("gone"));
}

TEST_CASE("local storage values are read from compressed table files and UTF-16 values")
{
    TempDir dir;
    std::u16string wide = u"dark";
    std::string utf16(1, '\0');
    for (char16_t c : wide) {
        utf16.push_back(static_cast<char>(c & 0xff));
        utf16.push_back(static_cast<char>(c >> 8));
    }
    const std::string internalKey = storageKey("theme") + fixed((5ull << 8) | 1, 8);
    const std::string data = snappyLiteral(block({{internalKey, utf16}}));
    std::string table = data + std::string(1, '\x01') + fixed(0, 4);
    const std::uint64_t indexOffset = table.size();
    const std::string index = block({{internalKey, varint(0) + varint(data.size())}});
    table += index + std::string(1, '\x00') + fixed(0, 4);
    std::string footer = varint(0) + varint(0) + varint(indexOffset) + varint(index.size());
    footer.resize(40, '\0');
    table += footer + fixed(0xdb4775248b80fb57ull, 8);
    writeBytes(dir.path() / "000005.ldb", table);

    const auto values = readChromiumLocalStorage(dir.path(), "file://");
    CHECK(values.at("theme") == "dark");
}
