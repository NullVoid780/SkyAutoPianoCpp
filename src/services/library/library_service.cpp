#include "services/library/library_service.hpp"

#include <QMetaObject>

#include <algorithm>
#include <cstring>
#include <utility>

#include "codec/sheet_exporter.hpp"
#include "core/base64.hpp"
#include "core/file_system.hpp"
#include "core/logger.hpp"
#include "core/string_utils.hpp"
#include "core/translator.hpp"
#include "model/key_table.hpp"
#include "services/library/legacy_import.hpp"
#include "services/library/playlist_service.hpp"

namespace sky {

namespace {

constexpr const char *kImportFallback = trNoop("The selected sheet could not be imported.");
constexpr const char *kInvalidSheetFile =
    trNoop("The sheet file is not valid, please try again with another file!");

std::string importMessage(const Error &error) {
    const std::string message(trimJs(error.userMessage));
    return message.empty() ? trUtf8(kImportFallback) : message;
}

Error importError(std::string message) {
    return Error{"IMPORT_FAILED", std::move(message), {}};
}

}  // namespace

LibraryService::PayloadReservation::PayloadReservation(std::shared_ptr<ReservationTracker> tracker,
                                                       std::string name)
    : m_tracker(std::move(tracker)), m_name(std::move(name)) {}

LibraryService::PayloadReservation::~PayloadReservation() {
    release();
}

LibraryService::PayloadReservation::PayloadReservation(PayloadReservation &&other) noexcept
    : m_tracker(std::move(other.m_tracker)), m_name(std::move(other.m_name)) {
    other.m_name.clear();
}

LibraryService::PayloadReservation &
LibraryService::PayloadReservation::operator=(PayloadReservation &&other) noexcept {
    if (this != &other) {
        release();
        m_tracker = std::move(other.m_tracker);
        m_name = std::move(other.m_name);
        other.m_name.clear();
    }
    return *this;
}

void LibraryService::PayloadReservation::release() {
    if (!m_tracker || m_name.empty()) {
        return;
    }
    std::lock_guard lock(m_tracker->mutex);
    m_tracker->names.erase(m_name);
    m_name.clear();
    m_tracker.reset();
}

LibraryService::LibraryService(std::filesystem::path dataDirectory, PlaylistService &playlists,
                               QObject *parent)
    : LibraryService(std::move(dataDirectory), playlists, StorageFileOps::native(), parent) {}

LibraryService::LibraryService(std::filesystem::path dataDirectory, PlaylistService &playlists,
                               StorageFileOps fileOps, QObject *parent)
    : QObject(parent), m_dataDirectory(std::move(dataDirectory)),
      m_listPath(m_dataDirectory / "listSheet.json"), m_playlists(playlists),
      m_fileOps(StorageFileOps::withDefaults(std::move(fileOps))),
      m_reservations(std::make_shared<ReservationTracker>()),
      m_pool(std::make_unique<QThreadPool>()) {
    qRegisterMetaType<sky::ImportSummary>();
    m_pool->setMaxThreadCount(2);
}

LibraryService::~LibraryService() {
    waitForBackgroundWork();
}

void LibraryService::waitForBackgroundWork() {
    m_pool->waitForDone();
}

void LibraryService::load() {
    m_entries.clear();
    m_payloads.clear();
    (void)m_fileOps.ensureDir(m_dataDirectory);
    if (!m_fileOps.exists(m_listPath)) {
        (void)writeList(m_entries);
        emit changed();
        return;
    }
    auto text = m_fileOps.readText(m_listPath);
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

Result<void> LibraryService::writeList(const std::vector<SheetEntry> &entries) const {
    OrderedJson array = OrderedJson::array();
    for (const SheetEntry &entry : entries) {
        array.push_back(entry.json());
    }
    if (auto ensured = m_fileOps.ensureDir(m_dataDirectory); !ensured) {
        Logger::error("library", ensured.error().details);
        return ensured.error();
    }
    auto written = m_fileOps.atomicWrite(m_listPath, dumpJson(array, 4));
    if (!written) {
        Logger::error("library", written.error().details);
    }
    return written;
}

const SheetEntry *LibraryService::entry(int index) const {
    if (index < 0 || index >= size()) {
        return nullptr;
    }
    return &m_entries[static_cast<std::size_t>(index)];
}

int LibraryService::indexOfPayload(const std::string &payloadFile) const {
    for (int i = 0; i < size(); ++i) {
        if (m_entries[static_cast<std::size_t>(i)].payloadFile() == payloadFile) {
            return i;
        }
    }
    return -1;
}

bool LibraryService::containsStoreSheet(const std::string &source,
                                        const std::string &sourceId) const {
    return std::any_of(m_entries.begin(), m_entries.end(), [&](const SheetEntry &entry) {
        return entry.source() == source && entry.sourceId() == sourceId;
    });
}

std::vector<StalePlaylistRef> LibraryService::stalePlaylistReferences() const {
    std::vector<StalePlaylistRef> stale;
    for (const Playlist &playlist : m_playlists.playlists()) {
        for (const std::string &key : playlist.sheetKeys) {
            if (indexOfPayload(key) < 0) {
                stale.push_back({playlist.id, playlist.name, key});
            }
        }
    }
    return stale;
}

std::filesystem::path LibraryService::payloadPath(const SheetEntry &entry) const {
    return m_dataDirectory / toWide(entry.payloadFile());
}

Result<std::shared_ptr<const StoredSheet>>
LibraryService::readPayload(const std::filesystem::path &file, double bitsPerPage) const {
    auto text = m_fileOps.readText(file);
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
    return std::shared_ptr<const StoredSheet>(
        std::make_shared<StoredSheet>(std::move(stored).value()));
}

Result<std::shared_ptr<const StoredSheet>> LibraryService::payload(int index) {
    const SheetEntry *row = entry(index);
    if (!row) {
        return Error{"SHEET_NOT_FOUND", trUtf8("Sheet not found."), {}};
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

void LibraryService::loadPayloadAsync(int index, PayloadCallback done) {
    const SheetEntry *row = entry(index);
    if (!row) {
        done(Error{"SHEET_NOT_FOUND", trUtf8("Sheet not found."), {}});
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

void LibraryService::invalidatePayload(int index) {
    if (index >= 0 && index < size()) {
        m_payloads[static_cast<std::size_t>(index)].reset();
    }
}

std::string LibraryService::payloadFileName(const std::string &sheetName, std::int64_t number) {
    std::string text = std::to_string(number);
    std::size_t pos = 0;
    while (pos < sheetName.size()) {
        const std::size_t start = pos;
        const char32_t cp = decodeUtf8At(sheetName, pos);
        const bool alnum =
            (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9');
        if (alnum) {
            text.append(sheetName, start, pos - start);
        } else {
            text.append(cp >= 0x10000 ? "--" : "-");
        }
    }
    return base64Encode(text) + ".json";
}

std::shared_ptr<LibraryService::PayloadReservation>
LibraryService::reserveUniqueName(const std::shared_ptr<ReservationTracker> &tracker,
                                  const std::filesystem::path &dataDirectory,
                                  const StorageFileOps &fileOps, const std::string &sheetName,
                                  const std::string &preferred, const std::string &avoid) {
    std::lock_guard lock(tracker->mutex);
    std::string candidate = preferred;
    auto available = [&](const std::string &name) {
        if (name.empty() || name == avoid || tracker->names.contains(name)) {
            return false;
        }
        return !fileOps.exists(dataDirectory / toWide(name));
    };
    while (!available(candidate)) {
        candidate = payloadFileName(sheetName, fileOps.nextSalt());
    }
    tracker->names.insert(candidate);
    return std::make_shared<PayloadReservation>(tracker, std::move(candidate));
}

LibraryService::PreparedImport
LibraryService::prepare(const std::filesystem::path &dataDirectory, const StorageFileOps &fileOps,
                        const std::shared_ptr<ReservationTracker> &reservations,
                        const std::string &displayName, Result<ParsedSheet> parsed,
                        const std::optional<StoreSource> &source) {
    PreparedImport prepared{displayName, Error{}, nullptr};
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

    auto reservation = reserveUniqueName(reservations, dataDirectory, fileOps, sheet.name);
    const std::string &fileName = reservation->name();

    const SheetPayload payload = stored.value().toPayload();
    if (auto written =
            fileOps.atomicWrite(dataDirectory / toWide(fileName), dumpJson(payload.toJson()));
        !written) {
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
    prepared.reservation = std::move(reservation);
    return prepared;
}

void LibraryService::removeCreatedPayloads(const std::vector<std::string> &payloadFiles) const {
    for (const std::string &fileName : payloadFiles) {
        if (fileName.empty() || indexOfPayload(fileName) >= 0) {
            continue;
        }
        const std::filesystem::path target = m_dataDirectory / toWide(fileName);
        if (!isInsideDirectory(target, m_dataDirectory)) {
            continue;
        }
        if (auto removed = m_fileOps.removeFile(target); !removed) {
            Logger::warning("library", "Failed to roll back uncommitted payload " + fileName +
                                           ": " + removed.error().details);
        }
    }
}

void LibraryService::importFiles(std::vector<std::filesystem::path> files) {
    if (files.empty()) {
        return;
    }
    QStringList started;
    for (const std::filesystem::path &file : files) {
        started << QString::fromStdWString(file.wstring());
    }
    emit importStarted(started);
    const std::filesystem::path dataDirectory = m_dataDirectory;
    const StorageFileOps fileOps = m_fileOps;
    const std::shared_ptr<ReservationTracker> reservations = m_reservations;
    m_pool->start([this, dataDirectory, fileOps, reservations, files = std::move(files)] {
        std::vector<PreparedImport> prepared;
        prepared.reserve(files.size());
        for (const std::filesystem::path &file : files) {
            const std::string name = fromWide(file.filename().wstring());
            auto bytes = fileOps.readBytes(file);
            Result<ParsedSheet> parsed =
                bytes ? parseSheet(bytes.value(), name) : Result<ParsedSheet>(bytes.error());
            prepared.push_back(prepare(dataDirectory, fileOps, reservations, name,
                                       std::move(parsed), std::nullopt));
        }
        QMetaObject::invokeMethod(
            this,
            [this, prepared = std::move(prepared)]() mutable {
                ImportSummary summary;
                summary.total = static_cast<int>(prepared.size());
                std::vector<SheetEntry> candidateEntries = m_entries;
                std::vector<std::string> createdPayloads;
                int staged = 0;
                for (const PreparedImport &item : prepared) {
                    if (item.row) {
                        candidateEntries.emplace_back(item.row.value());
                        createdPayloads.push_back(item.row.value()["keyMap"].get<std::string>());
                        ++staged;
                    } else {
                        summary.failures.push_back(
                            {item.displayName, item.row.error().userMessage});
                    }
                }
                if (staged > 0) {
                    if (auto saved = writeList(candidateEntries); saved) {
                        m_entries = std::move(candidateEntries);
                        m_payloads.resize(m_entries.size());
                        summary.succeeded = staged;
                        emit changed();
                    } else {
                        removeCreatedPayloads(createdPayloads);
                        for (const PreparedImport &item : prepared) {
                            if (item.row) {
                                summary.failures.push_back(
                                    {item.displayName, saved.error().userMessage});
                            }
                        }
                    }
                }
                prepared.clear();
                Logger::info("library", "import total=" + std::to_string(summary.total) +
                                            " ok=" + std::to_string(summary.succeeded));
                emit importFinished(summary);
            },
            Qt::QueuedConnection);
    });
}

void LibraryService::importStoreSheet(OrderedJson sheet, StoreSource source, ImportCallback done) {
    if (containsStoreSheet(source.source, source.sourceId)) {
        done(importError(trUtf8("This sheet is already in your library.")));
        return;
    }
    const auto encrypted = sheet.find("isEncrypted");
    if (encrypted != sheet.end() && jsTruthy(*encrypted)) {
        done(importError(trUtf8("Sheet has been encrypted!")));
        return;
    }
    const auto notes = sheet.find("songNotes");
    if (notes == sheet.end() || !jsTruthy(*notes)) {
        done(importError(trUtf8(kInvalidSheetFile)));
        return;
    }
    if (!notes->is_array() || notes->empty() || !(*notes)[0].is_object()) {
        done(importError(trUtf8("Sheet format is incorrect or still encrypted!")));
        return;
    }

    const std::filesystem::path dataDirectory = m_dataDirectory;
    const StorageFileOps fileOps = m_fileOps;
    const std::shared_ptr<ReservationTracker> reservations = m_reservations;
    m_pool->start([this, dataDirectory, fileOps, reservations, sheet = std::move(sheet), source,
                   done = std::move(done)] {
        const auto nameIt = sheet.find("name");
        const std::string baseName =
            nameIt != sheet.end() && jsTruthy(*nameIt) && nameIt->is_string()
                ? nameIt->get<std::string>()
                : "sheet";
        const std::string text = dumpJson(OrderedJson::array({sheet}));
        std::vector<std::byte> bytes(text.size());
        std::memcpy(bytes.data(), text.data(), text.size());
        PreparedImport prepared = prepare(dataDirectory, fileOps, reservations, baseName,
                                          parseSheet(bytes, baseName + ".json"), source);
        QMetaObject::invokeMethod(
            this,
            [this, prepared = std::move(prepared), source, done]() mutable {
                if (!prepared.row) {
                    done(prepared.row.error());
                    return;
                }
                const std::string createdFile = prepared.row.value()["keyMap"].get<std::string>();
                if (containsStoreSheet(source.source, source.sourceId)) {
                    removeCreatedPayloads({createdFile});
                    prepared.reservation.reset();
                    done(importError(trUtf8("This sheet is already in your library.")));
                    return;
                }
                std::vector<SheetEntry> candidateEntries = m_entries;
                candidateEntries.emplace_back(prepared.row.value());
                if (auto saved = writeList(candidateEntries); !saved) {
                    removeCreatedPayloads({createdFile});
                    prepared.reservation.reset();
                    done(saved.error());
                    return;
                }
                m_entries = std::move(candidateEntries);
                m_payloads.resize(m_entries.size());
                prepared.reservation.reset();
                emit changed();
                done(Result<void>());
            },
            Qt::QueuedConnection);
    });
}

DeleteSummary LibraryService::commitDelete(const std::vector<int> &targetIndices,
                                           std::vector<std::string> preFailedNames) {
    DeleteSummary summary;
    summary.failedNames = std::move(preFailedNames);

    std::set<int> indexSet;
    std::vector<std::string> targetKeys;
    std::vector<std::string> targetNames;
    for (int index : targetIndices) {
        const SheetEntry *row = entry(index);
        if (!row) {
            continue;
        }
        const std::string key = row->payloadFile();
        const std::filesystem::path file = payloadPath(*row);
        const bool inside = !key.empty() && isInsideDirectory(file, m_dataDirectory);
        if (!inside) {
            summary.failedNames.push_back(row->name().empty() ? key : row->name());
            continue;
        }
        if (indexSet.insert(index).second) {
            targetKeys.push_back(key);
            targetNames.push_back(row->name().empty() ? key : row->name());
        }
    }

    if (indexSet.empty()) {
        return summary;
    }

    std::vector<SheetEntry> candidateEntries;
    std::vector<std::shared_ptr<const StoredSheet>> candidatePayloads;
    candidateEntries.reserve(m_entries.size() - indexSet.size());
    candidatePayloads.reserve(m_payloads.size() - indexSet.size());
    for (int i = 0; i < size(); ++i) {
        if (!indexSet.contains(i)) {
            candidateEntries.push_back(m_entries[static_cast<std::size_t>(i)]);
            candidatePayloads.push_back(m_payloads[static_cast<std::size_t>(i)]);
        }
    }

    if (auto saved = writeList(candidateEntries); !saved) {
        summary.errorMessage = saved.error().userMessage;
        summary.failedNames.insert(summary.failedNames.end(), targetNames.begin(),
                                   targetNames.end());
        return summary;
    }

    m_entries = std::move(candidateEntries);
    m_payloads = std::move(candidatePayloads);
    summary.indexCommitted = true;
    summary.deletedKeys = targetKeys;
    emit changed();

    const std::set<std::string> deletedSet(targetKeys.begin(), targetKeys.end());
    if (auto playlistResult = m_playlists.removeSheetsEverywhere(deletedSet); !playlistResult) {
        summary.errorMessage = playlistResult.error().userMessage;
        Logger::warning("library",
                        "Library index committed deletion, but playlist update failed: " +
                            playlistResult.error().details);
        return summary;
    }
    summary.playlistUpdated = true;

    for (const std::string &key : targetKeys) {
        const std::filesystem::path file = m_dataDirectory / toWide(key);
        auto removed = m_fileOps.removeFile(file);
        if (!removed) {
            removed = m_fileOps.removeFile(file);
        }
        if (!removed) {
            summary.cleanupFailedKeys.push_back(key);
            if (summary.errorMessage.empty()) {
                summary.errorMessage = removed.error().userMessage;
            }
            Logger::warning("library", "Orphan payload left after committed deletion (" + key +
                                           "): " + removed.error().details);
        }
    }
    summary.payloadCleanupCompleted = summary.cleanupFailedKeys.empty();
    return summary;
}

DeleteSummary LibraryService::deleteSheet(int index) {
    if (!entry(index)) {
        DeleteSummary summary;
        summary.errorMessage = trUtf8("Sheet not found.");
        return summary;
    }
    return commitDelete({index}, {});
}

DeleteSummary LibraryService::deleteSheets(const std::set<std::string> &payloadFiles) {
    std::vector<int> indices;
    std::vector<std::string> failedNames;
    for (int i = 0; i < size(); ++i) {
        const SheetEntry &row = m_entries[static_cast<std::size_t>(i)];
        const std::string key = row.payloadFile();
        if (!payloadFiles.contains(key)) {
            continue;
        }
        const std::filesystem::path file = payloadPath(row);
        if (key.empty() || !isInsideDirectory(file, m_dataDirectory)) {
            failedNames.push_back(row.name().empty() ? key : row.name());
            continue;
        }
        indices.push_back(i);
    }
    return commitDelete(indices, std::move(failedNames));
}

Result<void> LibraryService::updateEntryField(int index, const std::string &field,
                                              OrderedJson value) {
    if (!entry(index)) {
        return Error{"SHEET_NOT_FOUND", trUtf8("Failed to save changes to the sheet"), {}};
    }
    std::vector<SheetEntry> candidateEntries = m_entries;
    candidateEntries[static_cast<std::size_t>(index)].set(field.c_str(), std::move(value));
    if (auto saved = writeList(candidateEntries); !saved) {
        return Error{"SHEET_SAVE_FAILED", trUtf8("Failed to save changes to the sheet"),
                     saved.error().details};
    }
    m_entries = std::move(candidateEntries);
    emit changed();
    return {};
}

Result<void> LibraryService::saveEditedNotes(int index, const SheetPayload &payload,
                                             bool hasHoldNotes) {
    const SheetEntry *row = entry(index);
    if (!row) {
        return Error{"SHEET_NOT_FOUND", trUtf8("Failed to save changes to the music sheet"), {}};
    }
    const bool rowChanged =
        row->hasHoldNotes() != hasHoldNotes || row->bitsPerPage() < kSkyKeyCount;
    const std::string payloadJson = dumpJson(payload.toJson());
    if (!rowChanged) {
        if (auto written = m_fileOps.atomicWrite(payloadPath(*row), payloadJson); !written) {
            return Error{"PAYLOAD_SAVE_FAILED", trUtf8("Failed to save changes to the music sheet"),
                         written.error().details};
        }
        invalidatePayload(index);
        emit payloadChanged(index);
        return {};
    }

    const std::string oldFile = row->payloadFile();
    const std::filesystem::path oldPath = payloadPath(*row);
    auto reservation =
        reserveUniqueName(m_reservations, m_dataDirectory, m_fileOps, row->name(), {}, oldFile);
    const std::string newFile = reservation->name();
    const std::filesystem::path newPath = m_dataDirectory / toWide(newFile);

    if (auto written = m_fileOps.atomicWrite(newPath, payloadJson); !written) {
        return Error{"PAYLOAD_SAVE_FAILED", trUtf8("Failed to save changes to the music sheet"),
                     written.error().details};
    }

    std::vector<SheetEntry> candidateEntries = m_entries;
    SheetEntry &candidate = candidateEntries[static_cast<std::size_t>(index)];
    candidate.set("keyMap", newFile);
    candidate.set("hasHoldNotes", hasHoldNotes);
    if (candidate.bitsPerPage() < kSkyKeyCount) {
        candidate.set("bitsPerPage", kSkyKeyCount);
    }

    if (auto saved = writeList(candidateEntries); !saved) {
        removeCreatedPayloads({newFile});
        return Error{"SHEET_SAVE_FAILED", trUtf8("Failed to save changes to the music sheet"),
                     saved.error().details};
    }

    m_entries = std::move(candidateEntries);
    invalidatePayload(index);
    reservation.reset();

    if (!oldFile.empty() && oldFile != newFile && isInsideDirectory(oldPath, m_dataDirectory)) {
        if (auto removed = m_fileOps.removeFile(oldPath); !removed) {
            Logger::warning("library", "Failed to remove previous payload after note edit (" +
                                           oldFile + "): " + removed.error().details);
        }
    }

    emit payloadChanged(index);
    emit changed();
    return {};
}

Toast LibraryService::importToast(const ImportSummary &summary) {
    if (summary.total > 1) {
        std::string text = fromQString(tr("Import complete. Success: %1.").arg(summary.succeeded));
        if (!summary.failures.empty()) {
            text += " " + fromQString(tr("Failed: %1. %2: %3")
                                          .arg(QString::number(summary.failures.size()),
                                               toQString(summary.failures.front().fileName),
                                               toQString(summary.failures.front().message)));
        }
        return Toast{summary.succeeded > 0 ? ToastType::Success : ToastType::Error,
                     {},
                     text,
                     kDefaultToastMs};
    }
    if (!summary.failures.empty()) {
        return Toast{ToastType::Error, {}, summary.failures.front().message, kDefaultToastMs};
    }
    if (summary.succeeded > 0) {
        return Toast{
            ToastType::Success, {}, trUtf8("Sheet imported successfully."), kDefaultToastMs};
    }
    return Toast{ToastType::Neutral, {}, {}, 0};
}

Toast LibraryService::deleteToast(const DeleteSummary &summary) {
    if (!summary.indexCommitted) {
        std::string detail;
        if (!summary.failedNames.empty() && !summary.failedNames.front().empty()) {
            detail = summary.failedNames.front();
        } else {
            detail = summary.errorMessage;
        }
        std::string text =
            fromQString(tr("Unable to delete the selected sheets. %1").arg(toQString(detail)));
        return Toast{ToastType::Error, {}, std::string(trimJs(text)), kDefaultToastMs};
    }
    if (!summary.playlistUpdated) {
        return Toast{ToastType::Warning,
                     {},
                     trUtf8("Deleted from library, but playlist changes could not be saved."),
                     kDefaultToastMs};
    }
    if (!summary.payloadCleanupCompleted) {
        return Toast{ToastType::Warning,
                     {},
                     trUtf8("Deleted from library, but some sheet files could not be removed."),
                     kDefaultToastMs};
    }
    const std::size_t deleted = summary.deletedKeys.size();
    if (!summary.failedNames.empty()) {
        std::string names;
        for (std::size_t i = 0; i < summary.failedNames.size(); ++i) {
            names += (i ? ", " : "") + summary.failedNames[i];
        }
        return Toast{ToastType::Warning,
                     {},
                     fromQString(tr("Deleted %1 sheet(s). Failed: %2.")
                                     .arg(QString::number(deleted), toQString(names))),
                     kDefaultToastMs};
    }
    return Toast{ToastType::Success,
                 {},
                 fromQString(trCount(static_cast<long long>(deleted), trNoop("Deleted 1 sheet."),
                                     trNoop("Deleted %1 sheets."))),
                 kDefaultToastMs};
}

std::string LibraryService::deleteConfirmation(std::size_t count) {
    return fromQString(trCount(static_cast<long long>(count),
                               trNoop("Delete 1 selected sheet? This cannot be undone."),
                               trNoop("Delete %1 selected sheets? This cannot be undone.")));
}

Result<void> LibraryService::writeExportFile(const std::filesystem::path &file,
                                             const std::string &content) {
    if (auto written = atomicWrite(file, content); !written) {
        return Error{"EXPORT_FAILED", trUtf8("Failed to export the sheet."),
                     written.error().details};
    }
    return {};
}

std::set<std::string> LibraryService::legacySongKeys() const {
    std::set<std::string> keys;
    for (const SheetEntry &entry : m_entries) {
        keys.insert(legacySongKey(entry.json()));
    }
    return keys;
}

void LibraryService::importLegacyRows(std::filesystem::path sourceData,
                                      std::vector<OrderedJson> rows, int alreadyPresent,
                                      std::function<void(LegacyImportSummary)> done) {
    const std::filesystem::path dataDirectory = m_dataDirectory;
    const StorageFileOps fileOps = m_fileOps;
    const std::shared_ptr<ReservationTracker> reservations = m_reservations;
    m_pool->start([this, dataDirectory, fileOps, reservations, sourceData = std::move(sourceData),
                   rows = std::move(rows), alreadyPresent, done = std::move(done)]() mutable {
        LegacyImportSummary summary;
        summary.alreadyPresent = alreadyPresent;
        std::vector<OrderedJson> copied;
        std::vector<std::shared_ptr<PayloadReservation>> heldReservations;
        std::vector<std::string> createdPayloads;
        for (OrderedJson &row : rows) {
            const std::string original = row["keyMap"].get<std::string>();
            const std::filesystem::path source = sourceData / toWide(original);
            if (original.find_first_of("/\\") != std::string::npos ||
                !fileOps.isRegularFile(source)) {
                ++summary.failed;
                continue;
            }
            const std::string name = row.value("name", std::string("sheet"));
            auto reservation =
                reserveUniqueName(reservations, dataDirectory, fileOps, name, original);
            const std::string target = reservation->name();
            if (!fileOps.copyFile(source, dataDirectory / toWide(target))) {
                ++summary.failed;
                continue;
            }
            heldReservations.push_back(std::move(reservation));
            createdPayloads.push_back(target);
            summary.payloadMap[original] = target;
            row["keyMap"] = target;
            copied.push_back(std::move(row));
        }
        QMetaObject::invokeMethod(
            this,
            [this, copied = std::move(copied), heldReservations = std::move(heldReservations),
             createdPayloads = std::move(createdPayloads), summary, done]() mutable {
                if (!copied.empty()) {
                    std::vector<SheetEntry> candidateEntries = m_entries;
                    for (OrderedJson &row : copied) {
                        candidateEntries.emplace_back(std::move(row));
                    }
                    if (auto saved = writeList(candidateEntries); saved) {
                        m_entries = std::move(candidateEntries);
                        m_payloads.resize(m_entries.size());
                        summary.imported = static_cast<int>(copied.size());
                        emit changed();
                    } else {
                        removeCreatedPayloads(createdPayloads);
                        summary.imported = 0;
                        summary.failed += static_cast<int>(copied.size());
                        summary.payloadMap.clear();
                        summary.indexCommitted = false;
                        summary.errorMessage = saved.error().userMessage;
                    }
                }
                heldReservations.clear();
                Logger::info("app", "legacy import imported=" + std::to_string(summary.imported) +
                                        " skipped=" + std::to_string(summary.alreadyPresent) +
                                        " failed=" + std::to_string(summary.failed));
                done(summary);
            },
            Qt::QueuedConnection);
    });
}

}  // namespace sky
