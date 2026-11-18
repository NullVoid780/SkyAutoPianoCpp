#pragma once

#include <string>
#include <vector>

#include <QWidget>

#include "model/playlist.hpp"

class QComboBox;
class QLabel;
class QPushButton;

namespace sky {

class IconButton;

class BulkToolbar final : public QWidget {
    Q_OBJECT

public:
    explicit BulkToolbar(QWidget *parent = nullptr);

    void setState(int selectedCount, bool allVisibleSelected, bool anyVisible);
    void setPlaylists(const std::vector<Playlist> &playlists, const std::string &selectedId);
    std::string selectedPlaylistId() const;

signals:
    void selectAllRequested();
    void addToPlaylistRequested();
    void deleteRequested();
    void doneRequested();
    void playlistChosen(const std::string &playlistId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyLayoutForWidth(int width);

    IconButton *m_selectAll;
    QLabel *m_summary;
    QComboBox *m_playlists;
    QPushButton *m_add;
    QPushButton *m_delete;
    QPushButton *m_done;
    bool m_allSelected = false;
    bool m_stacked = false;
};

}  // namespace sky
