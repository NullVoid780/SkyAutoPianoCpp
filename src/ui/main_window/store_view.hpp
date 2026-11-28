#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <QWidget>

#include "model/store_item.hpp"
#include "services/store/store_mapping.hpp"
#include "model/toast.hpp"

class QListView;
class QPushButton;
class QStandardItemModel;
class QTimer;

namespace sky {

class LibraryService;
class PlayerController;
class StateCard;
class StoreCardDelegate;
class StorePagination;
class StoreService;

class StoreView final : public QWidget {
    Q_OBJECT

public:
    StoreView(StoreService &store, LibraryService &library, PlayerController &player, QWidget *parent = nullptr);

    void activate();
    void setQuery(const QString &query);
    const QString &query() const { return m_query; }

signals:
    void toastRequested(const sky::Toast &toast);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void fetch(int page, bool refresh);
    void render();
    void play(const std::string &id);
    void save(const std::string &id);
    void prepare(const StoreItem &item, std::function<void(Result<PreparedStoreSheet>)> done);
    const StoreItem *itemById(const std::string &id) const;
    int totalPages() const;

    StoreService &m_store;
    LibraryService &m_library;
    PlayerController &m_player;
    QStandardItemModel *m_model;
    StoreCardDelegate *m_delegate;
    QListView *m_list;
    StateCard *m_state;
    QPushButton *m_retry;
    StorePagination *m_pagination;
    QTimer *m_debounce;

    QString m_query;
    std::vector<StoreItem> m_items;
    std::map<std::string, PreparedStoreSheet> m_prepared;
    int m_page = 1;
    std::int64_t m_total = 0;
    bool m_loading = false;
    bool m_loadingPage = false;
    bool m_hasLoaded = false;
    QString m_error;
    quint64 m_generation = 0;
    std::string m_playingId;
    std::string m_savingId;
};

}  // namespace sky
