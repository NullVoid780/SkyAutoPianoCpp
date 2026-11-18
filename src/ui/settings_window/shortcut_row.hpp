#pragma once

#include "ui/widgets/card_frame.hpp"

class QLabel;

namespace sky {

class ShortcutEdit;

class ShortcutRow final : public CardFrame {
    Q_OBJECT

public:
    ShortcutRow(const QString &iconId, const QString &label, QWidget *parent = nullptr);

    ShortcutEdit *field() const { return m_field; }

protected:
    void paintCard(QPainter &painter, const QRectF &body, qreal hover) override;
    qreal cornerRadius() const override { return 12; }

private:
    void refreshIcon();

    QString m_iconId;
    QLabel *m_icon;
    ShortcutEdit *m_field;
};

}  // namespace sky
