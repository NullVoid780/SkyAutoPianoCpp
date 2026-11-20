#pragma once

#include <QString>
#include <QWidget>

#include <vector>

namespace sky {

enum class SheetAction { Favorite, Edit, AddToPlaylist, MoveUp, MoveDown, RemoveFromPlaylist, Delete, Rename, Practice };

struct SheetActionEntry {
    SheetAction action;
    QString label;
    QString icon;
    bool enabled = true;
    bool destructive = false;
    bool favorite = false;
};

class SheetActionsMenu final : public QWidget {
    Q_OBJECT

public:
    explicit SheetActionsMenu(QWidget *parent = nullptr);

    void setEntries(std::vector<SheetActionEntry> entries);
    void popupBeside(const QRect &anchorGlobal, const QRect &windowGlobal);

signals:
    void triggered(sky::SheetAction action);
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QRectF panelRect() const;
    QRectF entryRect(int index) const;
    int entryAt(const QPointF &point) const;
    QSize panelSize() const;
    void activate(int index);

    std::vector<SheetActionEntry> m_entries;
    int m_hovered = -1;
};

}  // namespace sky
