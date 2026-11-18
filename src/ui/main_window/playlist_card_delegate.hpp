#pragma once

#include <QStyledItemDelegate>

namespace sky {

class PlaylistCardDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    enum Role { NameRole = Qt::UserRole + 1, CountRole, IdRole };

    static constexpr int kRowHeight = 98;

    explicit PlaylistCardDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    static QRectF cardRect(const QRect &rowRect);
    static QRectF actionRect(const QRect &rowRect);

    void setHover(int row, bool onAction);
    void setOpenMenuRow(int row);

private:
    int m_hoverRow = -1;
    bool m_hoverAction = false;
    int m_openMenuRow = -1;
};

}  // namespace sky
