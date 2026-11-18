#pragma once

#include "ui/widgets/card_frame.hpp"

class QLabel;

namespace sky {

class SettingCard final : public CardFrame {
    Q_OBJECT

public:
    enum class Variant { Normal, Warning };

    SettingCard(const QString &iconId, const QString &title, const QString &description, QWidget *control,
                Variant variant = Variant::Normal, QWidget *parent = nullptr);

    void setWarningText(const QString &text);

protected:
    void paintCard(QPainter &painter, const QRectF &body, qreal hover) override;
    std::vector<ShadowSpec> hoverShadows() const override;
    qreal cornerRadius() const override { return 16; }

private:
    QString m_iconId;
    Variant m_variant;
    QLabel *m_warning;
};

}  // namespace sky
