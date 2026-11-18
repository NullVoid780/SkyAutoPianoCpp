#pragma once

#include <string>

#include "services/notifications/toast_service.hpp"
#include "ui/core/overlay.hpp"

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace sky {

class IconButton;
class LibraryService;
class PlaylistService;

class PlaylistPicker final : public Overlay {
    Q_OBJECT

public:
    PlaylistPicker(PlaylistService &playlists, LibraryService &library, QWidget *window);

    void openForSheet(int libraryIndex);
    void openForNew();
    void openForRename(const std::string &playlistId);

signals:
    void toastRequested(const sky::Toast &toast);
    void playlistCreated(const std::string &playlistId);

protected:
    QWidget *panel() const override;
    QColor dimColor() const override;
    ShadowSpec panelShadow() const override;
    qreal panelRadius() const override;
    QRect panelGeometry() const override;
    void paintPanel(QPainter &painter, const QRectF &rect) override;

private:
    void present(const QString &title, const QString &submitLabel, const QString &name);
    void submit();
    void addTo(const std::string &playlistId);
    void renderRows();

    PlaylistService &m_playlists;
    LibraryService &m_library;
    QWidget *m_panel;
    QLabel *m_title;
    IconButton *m_close;
    QLineEdit *m_name;
    QPushButton *m_submit;
    QScrollArea *m_scroll;
    QWidget *m_rows;
    QVBoxLayout *m_rowsLayout;
    int m_sheetIndex = -1;
    std::string m_renameId;
};

}  // namespace sky
