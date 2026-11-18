#include "services/library/chromium_local_storage.hpp"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <vector>

#include "core/string_utils.hpp"

namespace sky {

namespace {

constexpr std::size_t kLogBlockSize = 32768;
constexpr std::size_t kLogHeaderSize = 7;
constexpr std::size_t kFooterSize = 48;
constexpr std::size_t kBlockTrailerSize = 5;
constexpr std::uint64_t kTableMagic = 0xdb4775248b80fb57ull;

struct Record {
    std::uint64_t sequence = 0;
    bool deleted = false;
    std::string value;
};

class Reader {
public:
    explicit Reader(std::string_view data) : m_data(data) {}

    bool varint(std::uint64_t &value)
    {
        value = 0;
        for (int shift = 0; shift <= 63 && m_pos < m_data.size(); shift += 7) {
            const auto byte = static_cast<unsigned char>(m_data[m_pos++]);
            value |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
            if (!(byte & 0x80)) {
                return true;
            }
        }
        return false;
    }

    bool bytes(std::size_t count, std::string_view &out)
    {
        if (m_data.size() - m_pos < count) {
            return false;
        }
        out = m_data.substr(m_pos, count);
        m_pos += count;
        return true;
    }

    bool fixed(int width, std::uint64_t &value)
    {
        std::string_view raw;
        if (!bytes(static_cast<std::size_t>(width), raw)) {
            return false;
        }
        value = 0;
        for (int i = width - 1; i >= 0; --i) {
            value = (value << 8) | static_cast<unsigned char>(raw[static_cast<std::size_t>(i)]);
        }
        return true;
    }

    bool lengthPrefixed(std::string_view &out)
    {
        std::uint64_t length = 0;
        return varint(length) && bytes(static_cast<std::size_t>(length), out);
    }

    std::size_t position() const { return m_pos; }
    bool atEnd() const { return m_pos >= m_data.size(); }

private:
    std::string_view m_data;
    std::size_t m_pos = 0;
};

std::string readWhole(const std::filesystem::path &file)
{
    std::ifstream input(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
}

void remember(std::map<std::string, Record> &records, std::string key, std::uint64_t sequence, bool deleted,
              std::string_view value)
{
    Record &record = records[std::move(key)];
    if (sequence >= record.sequence) {
        record = Record{sequence, deleted, std::string(value)};
    }
}

void parseWriteBatch(std::string_view batch, std::map<std::string, Record> &records)
{
    Reader reader(batch);
    std::uint64_t sequence = 0;
    std::uint64_t count = 0;
    if (!reader.fixed(8, sequence) || !reader.fixed(4, count)) {
        return;
    }
    for (std::uint64_t i = 0; i < count && !reader.atEnd(); ++i) {
        std::string_view tag;
        std::string_view key;
        if (!reader.bytes(1, tag) || !reader.lengthPrefixed(key)) {
            return;
        }
        if (tag[0] == 1) {
            std::string_view value;
            if (!reader.lengthPrefixed(value)) {
                return;
            }
            remember(records, std::string(key), sequence + i, false, value);
        } else {
            remember(records, std::string(key), sequence + i, true, {});
        }
    }
}

void readLogFile(const std::string &data, std::map<std::string, Record> &records)
{
    std::string pending;
    for (std::size_t block = 0; block < data.size(); block += kLogBlockSize) {
        std::size_t pos = block;
        const std::size_t end = std::min(data.size(), block + kLogBlockSize);
        while (pos + kLogHeaderSize <= end) {
            const auto length = static_cast<std::size_t>(static_cast<unsigned char>(data[pos + 4]) |
                                                         (static_cast<unsigned char>(data[pos + 5]) << 8));
            const auto type = static_cast<unsigned char>(data[pos + 6]);
            if (type == 0 || pos + kLogHeaderSize + length > end) {
                break;
            }
            const std::string_view payload(data.data() + pos + kLogHeaderSize, length);
            if (type == 1) {
                parseWriteBatch(payload, records);
                pending.clear();
            } else if (type == 2) {
                pending.assign(payload);
            } else if (type == 3) {
                pending.append(payload);
            } else if (type == 4) {
                pending.append(payload);
                parseWriteBatch(pending, records);
                pending.clear();
            }
            pos += kLogHeaderSize + length;
        }
    }
}

std::optional<std::string> readBlock(const std::string &table, std::uint64_t offset, std::uint64_t size)
{
    if (offset + size + kBlockTrailerSize > table.size()) {
        return std::nullopt;
    }
    const std::string_view contents(table.data() + offset, static_cast<std::size_t>(size));
    const auto compression = static_cast<unsigned char>(table[static_cast<std::size_t>(offset + size)]);
    if (compression == 0) {
        return std::string(contents);
    }
    if (compression == 1) {
        return snappyDecompress(contents);
    }
    return std::nullopt;
}

template <typename Visit>
void forEachBlockEntry(const std::string &block, Visit visit)
{
    if (block.size() < 4) {
        return;
    }
    Reader tail(std::string_view(block).substr(block.size() - 4));
    std::uint64_t restarts = 0;
    tail.fixed(4, restarts);
    const std::size_t limit = block.size() - 4 - static_cast<std::size_t>(restarts) * 4;
    if (limit > block.size()) {
        return;
    }
    Reader reader(std::string_view(block).substr(0, limit));
    std::string key;
    while (!reader.atEnd()) {
        std::uint64_t shared = 0;
        std::uint64_t unshared = 0;
        std::uint64_t valueLength = 0;
        std::string_view delta;
        std::string_view value;
        if (!reader.varint(shared) || !reader.varint(unshared) || !reader.varint(valueLength) ||
            shared > key.size() || !reader.bytes(static_cast<std::size_t>(unshared), delta) ||
            !reader.bytes(static_cast<std::size_t>(valueLength), value)) {
            return;
        }
        key.resize(static_cast<std::size_t>(shared));
        key.append(delta);
        visit(key, value);
    }
}

void readTableFile(const std::string &table, std::map<std::string, Record> &records)
{
    if (table.size() < kFooterSize) {
        return;
    }
    Reader footer(std::string_view(table).substr(table.size() - kFooterSize));
    std::uint64_t metaOffset = 0;
    std::uint64_t metaSize = 0;
    std::uint64_t indexOffset = 0;
    std::uint64_t indexSize = 0;
    std::uint64_t magic = 0;
    if (!footer.varint(metaOffset) || !footer.varint(metaSize) || !footer.varint(indexOffset) ||
        !footer.varint(indexSize)) {
        return;
    }
    Reader magicReader(std::string_view(table).substr(table.size() - 8));
    if (!magicReader.fixed(8, magic) || magic != kTableMagic) {
        return;
    }
    const auto index = readBlock(table, indexOffset, indexSize);
    if (!index) {
        return;
    }
    forEachBlockEntry(*index, [&](const std::string &, std::string_view handle) {
        Reader handleReader(handle);
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        if (!handleReader.varint(offset) || !handleReader.varint(size)) {
            return;
        }
        const auto data = readBlock(table, offset, size);
        if (!data) {
            return;
        }
        forEachBlockEntry(*data, [&](const std::string &internalKey, std::string_view value) {
            if (internalKey.size() < 8) {
                return;
            }
            Reader trailer(std::string_view(internalKey).substr(internalKey.size() - 8));
            std::uint64_t packed = 0;
            trailer.fixed(8, packed);
            remember(records, internalKey.substr(0, internalKey.size() - 8), packed >> 8, (packed & 0xff) == 0, value);
        });
    });
}

std::string decodeStorageValue(std::string_view raw)
{
    if (raw.empty()) {
        return {};
    }
    const std::string_view body = raw.substr(1);
    if (raw[0] == 0) {
        std::u16string text;
        for (std::size_t i = 0; i + 1 < body.size(); i += 2) {
            text.push_back(static_cast<char16_t>(static_cast<unsigned char>(body[i]) |
                                                 (static_cast<unsigned char>(body[i + 1]) << 8)));
        }
        return utf16ToUtf8(text);
    }
    return latin1ToUtf8(body);
}

}  // namespace

std::optional<std::string> snappyDecompress(std::string_view input)
{
    Reader reader(input);
    std::uint64_t expected = 0;
    if (!reader.varint(expected) || expected > (64ull << 20)) {
        return std::nullopt;
    }
    std::string output;
    output.reserve(static_cast<std::size_t>(expected));
    while (!reader.atEnd()) {
        std::string_view tagByte;
        reader.bytes(1, tagByte);
        const auto tag = static_cast<unsigned char>(tagByte[0]);
        const int kind = tag & 3;
        if (kind == 0) {
            std::uint64_t length = (tag >> 2) + 1u;
            if ((tag >> 2) >= 60) {
                const int extra = (tag >> 2) - 59;
                if (!reader.fixed(extra, length)) {
                    return std::nullopt;
                }
                ++length;
            }
            std::string_view literal;
            if (!reader.bytes(static_cast<std::size_t>(length), literal)) {
                return std::nullopt;
            }
            output.append(literal);
            continue;
        }
        std::uint64_t length = 0;
        std::uint64_t offset = 0;
        if (kind == 1) {
            std::uint64_t low = 0;
            if (!reader.fixed(1, low)) {
                return std::nullopt;
            }
            length = ((tag >> 2) & 7u) + 4u;
            offset = (static_cast<std::uint64_t>(tag >> 5) << 8) | low;
        } else {
            length = (tag >> 2) + 1u;
            if (!reader.fixed(kind == 2 ? 2 : 4, offset)) {
                return std::nullopt;
            }
        }
        if (offset == 0 || offset > output.size()) {
            return std::nullopt;
        }
        const std::size_t start = output.size() - static_cast<std::size_t>(offset);
        for (std::uint64_t i = 0; i < length; ++i) {
            output.push_back(output[start + static_cast<std::size_t>(i)]);
        }
    }
    if (output.size() != expected) {
        return std::nullopt;
    }
    return output;
}

std::map<std::string, std::string> readChromiumLocalStorage(const std::filesystem::path &levelDbDirectory,
                                                            std::string_view origin)
{
    std::map<std::string, Record> records;
    std::error_code error;
    std::vector<std::filesystem::path> logs;
    std::vector<std::filesystem::path> tables;
    for (const auto &entry : std::filesystem::directory_iterator(levelDbDirectory, error)) {
        const std::string extension = toLowerAscii(entry.path().extension().string());
        if (extension == ".log") {
            logs.push_back(entry.path());
        } else if (extension == ".ldb" || extension == ".sst") {
            tables.push_back(entry.path());
        }
    }
    for (const auto &table : tables) {
        readTableFile(readWhole(table), records);
    }
    for (const auto &log : logs) {
        readLogFile(readWhole(log), records);
    }

    const std::string prefix = "_" + std::string(origin) + std::string(1, '\0') + "\x01";
    std::map<std::string, std::string> values;
    for (const auto &[key, record] : records) {
        if (!record.deleted && key.rfind(prefix, 0) == 0) {
            values[key.substr(prefix.size())] = decodeStorageValue(record.value);
        }
    }
    return values;
}

std::filesystem::path electronLocalStorageDirectory()
{
    PWSTR roaming = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming))) {
        result = std::filesystem::path(roaming) / L"Sky Auto Piano" / L"Local Storage" / L"leveldb";
    }
    CoTaskMemFree(roaming);
    return result;
}

}  // namespace sky
