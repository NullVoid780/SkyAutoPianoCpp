#pragma once

#include <QAbstractButton>

class QVariantAnimation;

namespace sky {

class InlineSwitch final : public QAbstractButton {
    Q_OBJECT

public:
    explicit InlineSwitch(const QString &label, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    void setCheckedSilently(bool checked);

protected:
    void paintEvent(QPaintEvent *event) override;
    void checkStateSet() override;
    void nextCheckState() override;

private:
    void animate();

    QVariantAnimation *m_animation;
    qreal m_position = 0;
};

}  // namespace sky
