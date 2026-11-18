#pragma once

#include <QStyledItemDelegate>

namespace sky {

class StoreCardDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    enum Role {
        TitleRole = Qt::UserRole + 1,
        MetaRole,
        BadgesRole,
        StatusRole,
        CanPlayRole,
        CanSaveRole,
        PlayLabelRole,
        SaveLabelRole,
        IdRole,
    };
    enum class Button { None, Play, Save };

    explicit StoreCardDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    void setViewWidth(int width);
    void setHover(int row, Button button);
    void setPressed(int row, Button button);
    Button buttonAt(const QRect &rowRect, const QModelIndex &index, const QPointF &point) const;

private:
    struct Layout {
        QRectF card;
        QRectF title;
        QRectF meta;
        std::vector<QRectF> badges;
        QRectF status;
        QRectF play;
        QRectF save;
    };

    Layout layoutFor(const QRect &rowRect, const QModelIndex &index) const;
    void paintButton(QPainter &painter, const QRectF &rect, const QString &icon, const QString &label, bool play,
                     bool enabled, bool hovered, bool pressed) const;

    int m_viewWidth = 600;
    int m_hoverRow = -1;
    Button m_hoverButton = Button::None;
    int m_pressedRow = -1;
    Button m_pressedButton = Button::None;
};

}  // namespace sky
