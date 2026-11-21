#include "services/library/library_service.hpp"

#include <QMetaObject>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "codec/sheet_exporter.hpp"
#include "core/base64.hpp"
#include "core/file_system.hpp"
#include "core/logger.hpp"
#include "core/random.hpp"
#include "core/string_utils.hpp"
#include "core/text_file.hpp"
#include "services/library/legacy_import.hpp"
#include "services/library/playlist_service.hpp"

namespace sky {

namespace {

constexpr const char *kImportFallback = "The selected sheet could not be imported.";
constexpr const char *kInvalidSheetFile = "The sheet file is not valid, please try again with another file!";

std::string importMessage(const Error &error)
{
    const std::string message(trimJs(error.userMessage));
    return message.empty() ? std::string(kImportFallback) : message;
}

Error importError(std::string message)
{
    return Error{"IMPORT_FAILED", std::move(message), {}};
}

}  // namespace

LibraryService::LibraryService(std::filesystem::path dataDirectory, PlaylistService &playlists, QObject *parent)
    : QObject(parent),
      m_dataDirectory(std::move(dataDirectory)),
      m_listPath(m_dataDirectory / "listSheet.json"),
      m_playlists(playlists),
      m_pool(std::make_unique<QThreadPool>())
{
    qRegisterMetaType<sky::ImportSummary>();
    m_pool->setMaxThreadCount(2);
}

LibraryService::~LibraryService()
{
    waitForBackgroundWork();
}

void LibraryService::waitForBackgroundWork()
{
    m_pool->waitForDone();
}

void LibraryService::load()
{
    m_entries.clear();
    m_payloads.clear();
    (void)ensureDir(m_dataDirectory);
    std::error_code ec;
    if (!std::filesystem::exists(m_listPath, ec)) {
        (void)saveList();
        emit changed();
        return;
    }
    auto text = readTextFile(m_listPath);
    auto parsed = text ? parseJson<OrderedJson>(text.value()) : Result<OrderedJson>(text.error());
    if (!parsed) {
        Logger::error("library", "Failed to parse listSheet.json: " + parsed.error().userMessage);
    } else if (parsed.value().is_array()) {
        for (OrderedJson &row : parsed.value()) {
            m_entries.emplace_back(std::move(row));
        }
    }
    m_payloads.resize(m_entries.size());
    emit changed();
}

Result<void> LibraryService::saveList()
{
    OrderedJson array = OrderedJson::array();
    for (const SheetEntry &entry : m_entries) {
        array.push_back(entry.json());
    }
    auto written = atomicWrite(m_listPath, dumpJson(array, 4));
    if (!written) {
        Logger::error("library", written.error().details);
    }
    return written;
}

const SheetEntry *LibraryService::entry(int index) const
{
    if (index < 0 || index >= size()) {
        return nullptr;
    }
    return &m_entries[static_cast<std::size_t>(index)];
}

int LibraryService::indexOfPayload(const std::string &payloadFile) const
{
    for (int i = 0; i < size(); ++i) {
        if (m_entries[static_cast<std::size_t>(i)].payloadFile() == payloadFile) {
            return i;
        }
    }
    return -1;
}

bool LibraryService::containsStoreSheet(const std::string &source, const std::string &sourceId) const
{
    return std::any_of(m_entries.begin(), m_entries.end(), [&](const SheetEntry &entry) {
        return entry.source() == source && entry.sourceId() == sourceId;
    });
}

std::filesystem::path LibraryService::payloadPath(const SheetEntry &entry) const
{
    return m_dataDirectory / toWide(entry.payloadFile());
}

Result<std::shared_ptr<const StoredSheet>> LibraryService::readPayload(const std::filesystem::path &file,
                                                                        double bitsPerPage)
{
    auto text = readTextFile(file);
    if (!text) {
        return text.error();
    }
    auto parsed = parseJson<OrderedJson>(text.value());
    if (!parsed) {
        return parsed.error();
    }
    auto stored = loadStoredSheet(parsed.value(), bitsPerPage);
    if (!stored) {
        return stored.error();
    }
    return std::shared_ptr<const StoredSheet>(std::make_shared<StoredSheet>(std::move(stored).value()));
}

Result<std::shared_ptr<const StoredSheet>> LibraryService::payload(int index)
{
    const SheetEntry *row = entry(index);
    if (!row) {
        return Error{"SHEET_NOT_FOUND", "Sheet not found.", {}};
    }
    auto &cached = m_payloads[static_cast<std::size_t>(index)];
    if (cached) {
        return cached;
    }
    auto loaded = readPayload(payloadPath(*row), row->bitsPerPage());
    if (loaded) {
        cached = loaded.value();
    }
    return loaded;
}

void LibraryService::loadPayloadAsync(int index, PayloadCallback done)
{
    const SheetEntry *row = entry(index);
    if (!row) {
        done(Error{"SHEET_NOT_FOUND", "Sheet not found.", {}});
        return;
    }
    if (const auto &cached = m_payloads[static_cast<std::size_t>(index)]) {
        done(cached);
        return;
    }
    const std::string key = row->payloadFile();
    const std::filesystem::path file = payloadPath(*row);
    const double bitsPerPage = row->bitsPerPage();
    m_pool->start([this, key, file, bitsPerPage, done = std::move(done)] {
        auto loaded = readPayload(file, bitsPerPage);
        QMetaObject::invokeMethod(
            this,
            [this, key, loaded = std::move(loaded), done] {
                const int current = indexOfPayload(key);
                if (loaded && current >= 0) {
                    m_payloads[static_cast<std::size_t>(current)] = loaded.value();
                }
                done(loaded);
            },
            Qt::QueuedConnection);
    });
}

void LibraryService::invalidatePayload(int index)
{
    if (index >= 0 && index < size()) {
        m_payloads[static_cast<std::size_t>(index)].reset();
    }
}

std::string LibraryService::payloadFileName(const std::string &sheetName, std::int64_t number)
{
    std::string text = std::to_string(number);
    std::size_t pos = 0;
    while (pos < sheetName.size()) {
        const std::size_t start = pos;
        const char32_t cp = decodeUtf8At(sheetName, pos);
        const bool alnum = (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9');
        if (alnum) {
            text.append(sheetName, start, pos - start);
        } else {
            text.append(cp >= 0x10000 ? "--" : "-");
        }
    }
    return base64Encode(text) + ".json";
}

LibraryService::PreparedImport LibraryService::prepare(const std::filesystem::path &dataDirectory,
                                                       const std::string &displayName, Result<ParsedSheet> parsed,
                                                       const std::optional<StoreSource> &source,
                                                       std::set<std::string> &reserved)
{
    PreparedImport prepared{displayName, Error{}};
    if (!parsed) {
        prepared.row = importError(importMessage(parsed.error()));
        return prepared;
    }
    const Sheet &sheet = parsed.value().sheet;
    auto stored = loadStoredSheet(parsed.value().storage.toJson(), sheet.bitsPerPage);
    if (!stored) {
        prepared.row = importError(importMessage(stored.error()));
        return prepared;
    }

    std::string fileName;
    std::error_code ec;
    do {
        fileName = payloadFileName(sheet.name, static_cast<std::int64_t>(std::floor(randomUnit() * 10000)) + 1);
    } while (reserved.contains(fileName) || std::filesystem::exists(dataDirectory / toWide(fileName), ec));
    reserved.insert(fileName);

    const SheetPayload payload = stored.value().toPayload();
    if (auto written = atomicWrite(dataDirectory / toWide(fileName), dumpJson(payload.toJson())); !written) {
        prepared.row = importError(written.error().userMessage);
        return prepared;
    }

    OrderedJson row = OrderedJson::object();
    row["name"] = sheet.name;
    row["author"] = sheet.author.empty() ? "Unknown" : sheet.author;
    row["transcribedBy"] = sheet.transcribedBy.empty() ? "Unknown" : sheet.transcribedBy;
    row["bpm"] = jsonNumber(sheet.bpm);
    row["bitsPerPage"] = stored.value().bitsPerPage;
    row["pitchLevel"] = jsonNumber(sheet.pitchLevel);
    row["isComposed"] = sheet.isComposed;
    row["keyMap"] = fileName;
    row["hasHoldNotes"] = stored.value().hasHoldNotes;
    if (source) {
        row["source"] = source->source;
        row["sourceId"] = source->sourceId;
        row["sourceUrl"] = source->sourceUrl;
    }
    prepared.row = std::move(row);
    return prepared;
}

void LibraryService::importFiles(std::vector<std::filesystem::path> files)
{
    if (files.empty()) {
        return;
    }
    QStringList started;
    for (const std::filesystem::path &file : files) {
        started << QString::fromStdWString(file.wstring());
    }
    emit importStarted(started);
    const std::filesystem::path dataDirectory = m_dataDirectory;
    m_pool->start([this, dataDirectory, files = std::move(files)] {
        std::set<std::string> reserved;
        std::vector<PreparedImport> prepared;
        for (const std::filesystem::path &file : files) {
            const std::string name = fromWide(file.filename().wstring());
            auto bytes = readFileBytes(file);
            Result<ParsedSheet> parsed = bytes ? parseSheet(bytes.value(), name) : Result<ParsedSheet>(bytes.error());
            prepared.push_back(prepare(dataDirectory, name, std::move(parsed), std::nullopt, reserved));
        }
        QMetaObject::invokeMethod(
            this,
            [this, prepared = std::move(prepared)] {
                ImportSummary summary;
                summary.total = static_cast<int>(prepared.size());
                for (const PreparedImport &item : prepared) {
                    if (item.row) {
                        m_entries.emplace_back(item.row.value());
                        m_payloads.emplace_back();
                        ++summary.succeeded;
                    } else {
                        summary.failures.push_back({item.displayName, item.row.error().userMessage});
                    }
                }
                if (summary.succeeded > 0) {
                    (void)saveList();
                    emit changed();
                }
                Logger::info("library", "import total=" + std::to_string(summary.total) +
                                            " ok=" + std::to_string(summary.succeeded));
                emit importFinished(summary);
            },
            Qt::QueuedConnection);
    });
}

void LibraryService::importStoreSheet(OrderedJson sheet, StoreSource source, ImportCallback done)
{
    if (containsStoreSheet(source.source, source.sourceId)) {
        done(importError("This sheet is already in your library."));
        return;
    }
    const auto encrypted = sheet.find("isEncrypted");
    if (encrypted != sheet.end() && jsTruthy(*encrypted)) {
        done(importError("Sheet has been encrypted!"));
        return;
    }
    const auto notes = sheet.find("songNotes");
    if (notes == sheet.end() || !jsTruthy(*notes)) {
        done(importError(kInvalidSheetFile));
        return;
    }
    if (!notes->is_array() || notes->empty() || !(*notes)[0].is_object()) {
        done(importError("Sheet format is incorrect or still encrypted!"));
        return;
    }

    const std::filesystem::path dataDirectory = m_dataDirectory;
    m_pool->start([this, dataDirectory, sheet = std::move(sheet), source, done = std::move(done)] {
        const auto nameIt = sheet.find("name");
        const std::string baseName =
            nameIt != sheet.end() && jsTruthy(*nameIt) && nameIt->is_string() ? nameIt->get<std::string>() : "sheet";
        const std::string text = dumpJson(OrderedJson::array({sheet}));
        std::vector<std::byte> bytes(text.size());
        std::memcpy(bytes.data(), text.data(), text.size());
        std::set<std::string> reserved;
        PreparedImport prepared =
            prepare(dataDirectory, baseName, parseSheet(bytes, baseName + ".json"), source, reserved);
        QMetaObject::invokeMethod(
            this,
            [this, prepared = std::move(prepared), source, done] {
                if (!prepared.row) {
                    done(prepared.row.error());
                    return;
                }
                if (containsStoreSheet(source.source, source.sourceId)) {
                    (void)safeDelete(m_dataDirectory / toWide(prepared.row.value()["keyMap"].get<std::string>()));
                    done(importError("This sheet is already in your library."));
                    return;
                }
                m_entries.emplace_back(prepared.row.value());
                m_payloads.emplace_back();
                (void)saveList();
                emit changed();
                done(Result<void>());
            },
            Qt::QueuedConnection);
    });
}

Result<void> LibraryService::deleteSheet(int index)
{
    const SheetEntry *row = entry(index);
    if (!row) {
        return {};
    }
    const std::string key = row->payloadFile();
    const std::filesystem::path file = payloadPath(*row);
    if (!key.empty() && isInsideDirectory(file, m_dataDirectory)) {
        if (auto removed = safeDelete(file); !removed) {
            return removed;
        }
    }
    m_playlists.removeSheetsEverywhere({key});
    m_entries.erase(m_entries.begin() + index);
    m_payloads.erase(m_payloads.begin() + index);
    auto saved = saveList();
    emit changed();
    return saved;
}

DeleteSummary LibraryService::deleteSheets(const std::set<std::string> &payloadFiles)
{
    DeleteSummary summary;
    std::set<std::string> deleted;
    for (const SheetEntry &row : m_entries) {
        const std::string key = row.payloadFile();
        if (!payloadFiles.contains(key)) {
            continue;
        }
        const std::filesystem::path file = payloadPath(row);
        const bool inside = !key.empty() && isInsideDirectory(file, m_dataDirectory);
        if (inside && safeDelete(file)) {
            deleted.insert(key);
            summary.deletedKeys.push_back(key);
        } else {
            summary.failedNames.push_back(row.name().empty() ? key : row.name());
        }
    }
    if (deleted.empty()) {
        return summary;
    }
    for (std::size_t i = m_entries.size(); i-- > 0;) {
        if (deleted.contains(m_entries[i].payloadFile())) {
            m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(i));
            m_payloads.erase(m_payloads.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
    m_playlists.removeSheetsEverywhere(deleted);
    (void)saveList();
    emit changed();
    return summary;
}

Result<void> LibraryService::updateEntryField(int index, const std::string &field, OrderedJson value)
{
    if (!entry(index)) {
        return Error{"SHEET_NOT_FOUND", "Failed to save changes to the sheet", {}};
    }
    m_entries[static_cast<std::size_t>(index)].set(field.c_str(), std::move(value));
    if (auto saved = saveList(); !saved) {
        return Error{"SHEET_SAVE_FAILED", "Failed to save changes to the sheet", saved.error().details};
    }
    emit changed();
    return {};
}

Result<void> LibraryService::saveEditedKeyMap(int index, const KeyMap &keyMap)
{
    const SheetEntry *row = entry(index);
    if (!row) {
        return Error{"SHEET_NOT_FOUND", "Failed to save changes to the music sheet", {}};
    }
    const SheetPayload payload{keyMap, songNotesFromKeyMap(keyMap)};
    if (auto written = atomicWrite(payloadPath(*row), dumpJson(payload.toJson())); !written) {
        return Error{"PAYLOAD_SAVE_FAILED", "Failed to save changes to the music sheet", written.error().details};
    }
    invalidatePayload(index);
    emit payloadChanged(index);
    return {};
}

Toast LibraryService::importToast(const ImportSummary &summary)
{
    if (summary.total > 1) {
        std::string text = "Import complete. Success: " + std::to_string(summary.succeeded) + ".";
        if (!summary.failures.empty()) {
            text += " Failed: " + std::to_string(summary.failures.size()) + ". " + summary.failures.front().fileName +
                    ": " + summary.failures.front().message;
        }
        return Toast{summary.succeeded > 0 ? ToastType::Success : ToastType::Error, {}, text, kDefaultToastMs};
    }
    if (!summary.failures.empty()) {
        return Toast{ToastType::Error, {}, summary.failures.front().message, kDefaultToastMs};
    }
    if (summary.succeeded > 0) {
        return Toast{ToastType::Success, {}, "Sheet imported successfully.", kDefaultToastMs};
    }
    return Toast{ToastType::Neutral, {}, {}, 0};
}

Toast LibraryService::deleteToast(const DeleteSummary &summary)
{
    const std::size_t deleted = summary.deletedKeys.size();
    if (deleted == 0) {
        std::string text = "Unable to delete the selected sheets. " +
                           (summary.failedNames.empty() ? std::string() : summary.failedNames.front());
        return Toast{ToastType::Error, {}, std::string(trimJs(text)), kDefaultToastMs};
    }
    if (!summary.failedNames.empty()) {
        std::string names;
        for (std::size_t i = 0; i < summary.failedNames.size(); ++i) {
            names += (i ? ", " : "") + summary.failedNames[i];
        }
        return Toast{ToastType::Warning, {},
                     "Deleted " + std::to_string(deleted) + " sheet(s). Failed: " + names + ".", kDefaultToastMs};
    }
    return Toast{ToastType::Success, {},
                 "Deleted " + std::to_string(deleted) + (deleted == 1 ? " sheet." : " sheets."), kDefaultToastMs};
}

std::string LibraryService::deleteConfirmation(std::size_t count)
{
    return "Delete " + std::to_string(count) + " selected sheet" + (count == 1 ? "" : "s") + "? This cannot be undone.";
}


Result<void> LibraryService::writeExportFile(const std::filesystem::path &file, const std::string &content)
{
    if (auto written = atomicWrite(file, content); !written) {
        return Error{"EXPORT_FAILED", "Failed to export the sheet.", written.error().details};
    }
    return {};
}


std::set<std::string> LibraryService::legacySongKeys() const
{
    std::set<std::string> keys;
    for (const SheetEntry &entry : m_entries) {
        keys.insert(legacySongKey(entry.json()));
    }
    return keys;
}

void LibraryService::importLegacyRows(std::filesystem::path sourceData, std::vector<OrderedJson> rows,
                                      int alreadyPresent, std::function<void(LegacyImportSummary)> done)
{
    const std::filesystem::path dataDirectory = m_dataDirectory;
    m_pool->start([this, dataDirectory, sourceData = std::move(sourceData), rows = std::move(rows), alreadyPresent,
                   done = std::move(done)]() mutable {
        LegacyImportSummary summary;
        summary.alreadyPresent = alreadyPresent;
        std::vector<OrderedJson> copied;
        std::set<std::string> reserved;
        std::error_code error;
        for (OrderedJson &row : rows) {
            const std::string original = row["keyMap"].get<std::string>();
            const std::filesystem::path source = sourceData / toWide(original);
            if (original.find_first_of("/\\") != std::string::npos ||
                !std::filesystem::is_regular_file(source, error)) {
                ++summary.failed;
                continue;
            }
            std::string target = original;
            const std::string name = row.value("name", std::string("sheet"));
            while (reserved.contains(target) || std::filesystem::exists(dataDirectory / toWide(target), error)) {
                target = payloadFileName(name, static_cast<std::int64_t>(std::floor(randomUnit() * 10000)) + 1);
            }
            if (!std::filesystem::copy_file(source, dataDirectory / toWide(target), error)) {
                ++summary.failed;
                continue;
            }
            reserved.insert(target);
            summary.payloadMap[original] = target;
            row["keyMap"] = target;
            copied.push_back(std::move(row));
        }
        QMetaObject::invokeMethod(
            this,
            [this, copied = std::move(copied), summary, done]() mutable {
                for (OrderedJson &row : copied) {
                    m_entries.emplace_back(std::move(row));
                    m_payloads.emplace_back();
                    ++summary.imported;
                }
                if (summary.imported > 0) {
                    if (auto saved = saveList(); !saved) {
                        Logger::error("library", saved.error().details);
                    }
                    emit changed();
                }
                Logger::info("app", "legacy import imported=" + std::to_string(summary.imported) +
                                        " skipped=" + std::to_string(summary.alreadyPresent) +
                                        " failed=" + std::to_string(summary.failed));
                done(summary);
            },
            Qt::QueuedConnection);
    });
}

}  // namespace sky
