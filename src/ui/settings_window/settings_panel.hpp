#pragma once

#include "ui/widgets/card_frame.hpp"

namespace sky {

class SettingsPanel final : public CardFrame {
    Q_OBJECT

public:
    explicit SettingsPanel(int padding, QWidget *parent = nullptr);

protected:
    void paintCard(QPainter &painter, const QRectF &body, qreal hover) override;
    qreal cornerRadius() const override { return 16; }
};

}  // namespace sky
