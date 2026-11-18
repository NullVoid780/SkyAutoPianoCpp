#pragma once

#include <QWidget>

class QLabel;

namespace sky {

class IconButton;

class PlaylistDetailBar final : public QWidget {
    Q_OBJECT

public:
    explicit PlaylistDetailBar(QWidget *parent = nullptr);

    void setPlaylist(const QString &name, int sheetCount);

signals:
    void backRequested();

private:
    IconButton *m_back;
    QLabel *m_title;
    QLabel *m_subtitle;
};

}  // namespace sky
