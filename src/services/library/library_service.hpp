#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <QObject>
#include <QStringList>
#include <QThreadPool>

#include "codec/sheet_codec.hpp"
#include "core/json.hpp"
#include "core/result.hpp"
#include "model/sheet_entry.hpp"
#include "model/stored_sheet.hpp"
#include "model/toast.hpp"
#include "services/library/storage_ops.hpp"

namespace sky {

class PlaylistService;

struct StoreSource {
    std::string source = "sky-sheet-store";
    std::string sourceId;
    std::string sourceUrl;
};

struct ImportFailure {
    std::string fileName;
    std::string message;
};

struct ImportSummary {
    int total = 0;
    int succeeded = 0;
    std::vector<ImportFailure> failures;
};

struct LegacyImportSummary {
    int imported = 0;
    int alreadyPresent = 0;
    int failed = 0;
    std::map<std::string, std::string> payloadMap;
    bool indexCommitted = true;
    std::string errorMessage;
};

struct DeleteSummary {
    std::vector<std::string> deletedKeys;
    std::vector<std::string> failedNames;
    std::vector<std::string> cleanupFailedKeys;
    bool indexCommitted = false;
    bool playlistUpdated = false;
    bool payloadCleanupCompleted = false;
    std::string errorMessage;

    bool ok() const {
        return indexCommitted && playlistUpdated && payloadCleanupCompleted && failedNames.empty();
    }
};

struct StalePlaylistRef {
    std::string playlistId;
    std::string playlistName;
    std::string sheetKey;
};

class LibraryService final : public QObject {
    Q_OBJECT

public:
    using PayloadCallback = std::function<void(Result<std::shared_ptr<const StoredSheet>>)>;
    using ImportCallback = std::function<void(Result<void>)>;

    LibraryService(std::filesystem::path dataDirectory, PlaylistService &playlists,
                   QObject *parent = nullptr);
    LibraryService(std::filesystem::path dataDirectory, PlaylistService &playlists,
                   StorageFileOps fileOps, QObject *parent = nullptr);
    ~LibraryService() override;

    void waitForBackgroundWork();

    void load();
    const std::vector<SheetEntry> &entries() const { return m_entries; }
    int size() const { return static_cast<int>(m_entries.size()); }
    const SheetEntry *entry(int index) const;
    int indexOfPayload(const std::string &payloadFile) const;
    bool containsStoreSheet(const std::string &source, const std::string &sourceId) const;
    std::vector<StalePlaylistRef> stalePlaylistReferences() const;

    Result<std::shared_ptr<const StoredSheet>> payload(int index);
    void loadPayloadAsync(int index, PayloadCallback done);
    void invalidatePayload(int index);

    void importFiles(std::vector<std::filesystem::path> files);
    void importStoreSheet(OrderedJson sheet, StoreSource source, ImportCallback done);
    DeleteSummary deleteSheet(int index);
    DeleteSummary deleteSheets(const std::set<std::string> &payloadFiles);
    Result<void> updateEntryField(int index, const std::string &field, OrderedJson value);
    Result<void> saveEditedNotes(int index, const SheetPayload &payload, bool hasHoldNotes);
    std::set<std::string> legacySongKeys() const;
    void importLegacyRows(std::filesystem::path sourceData, std::vector<OrderedJson> rows,
                          int alreadyPresent, std::function<void(LegacyImportSummary)> done);

    const std::filesystem::path &dataDirectory() const { return m_dataDirectory; }

    static std::string payloadFileName(const std::string &sheetName, std::int64_t number);
    static Toast importToast(const ImportSummary &summary);
    static Toast deleteToast(const DeleteSummary &summary);
    static std::string deleteConfirmation(std::size_t count);
    static Result<void> writeExportFile(const std::filesystem::path &file,
                                        const std::string &content);

signals:
    void changed();
    void payloadChanged(int index);
    void importFinished(const sky::ImportSummary &summary);
    void importStarted(const QStringList &files);

private:
    struct ReservationTracker {
        std::mutex mutex;
        std::set<std::string> names;
    };

    class PayloadReservation {
    public:
        PayloadReservation() = default;
        PayloadReservation(std::shared_ptr<ReservationTracker> tracker, std::string name);
        ~PayloadReservation();

        PayloadReservation(const PayloadReservation &) = delete;
        PayloadReservation &operator=(const PayloadReservation &) = delete;
        PayloadReservation(PayloadReservation &&other) noexcept;
        PayloadReservation &operator=(PayloadReservation &&other) noexcept;

        const std::string &name() const { return m_name; }
        void release();

    private:
        std::shared_ptr<ReservationTracker> m_tracker;
        std::string m_name;
    };

    struct PreparedImport {
        std::string displayName;
        Result<OrderedJson> row = Error{};
        std::shared_ptr<PayloadReservation> reservation;
    };

    static std::shared_ptr<PayloadReservation>
    reserveUniqueName(const std::shared_ptr<ReservationTracker> &tracker,
                      const std::filesystem::path &dataDirectory, const StorageFileOps &fileOps,
                      const std::string &sheetName, const std::string &preferred = {},
                      const std::string &avoid = {});

    static PreparedImport prepare(const std::filesystem::path &dataDirectory,
                                  const StorageFileOps &fileOps,
                                  const std::shared_ptr<ReservationTracker> &reservations,
                                  const std::string &displayName, Result<ParsedSheet> parsed,
                                  const std::optional<StoreSource> &source);

    Result<void> writeList(const std::vector<SheetEntry> &entries) const;
    DeleteSummary commitDelete(const std::vector<int> &targetIndices,
                               std::vector<std::string> preFailedNames);
    void removeCreatedPayloads(const std::vector<std::string> &payloadFiles) const;
    std::filesystem::path payloadPath(const SheetEntry &entry) const;
    Result<std::shared_ptr<const StoredSheet>> readPayload(const std::filesystem::path &file,
                                                           double bitsPerPage) const;

    std::filesystem::path m_dataDirectory;
    std::filesystem::path m_listPath;
    PlaylistService &m_playlists;
    StorageFileOps m_fileOps;
    std::vector<SheetEntry> m_entries;
    std::vector<std::shared_ptr<const StoredSheet>> m_payloads;
    std::shared_ptr<ReservationTracker> m_reservations;
    std::unique_ptr<QThreadPool> m_pool;
};

}  // namespace sky

Q_DECLARE_METATYPE(sky::ImportSummary)
