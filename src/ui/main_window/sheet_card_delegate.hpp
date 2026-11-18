#pragma once

#include <QPersistentModelIndex>
#include <QStyledItemDelegate>

namespace sky {

class SheetCardDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    static constexpr int kRowHeight = 160;

    explicit SheetCardDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    static QRectF cardRect(const QRect &rowRect);
    static QRectF actionRect(const QRect &rowRect);
    static QRectF checkRect(const QRect &rowRect);

    void setHover(int row, bool onAction);
    void setOpenMenuRow(int row);
    void setCurrentRow(int row);

private:
    void paintInfoRow(QPainter &painter, qreal left, qreal top, qreal width, const QString &icon, const QColor &tint,
                      const QString &label, const QString &value) const;

    int m_hoverRow = -1;
    bool m_hoverAction = false;
    int m_openMenuRow = -1;
    int m_currentRow = -1;
};

}  // namespace sky
