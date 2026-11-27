#pragma once

#include <QAbstractButton>

namespace sky {

class HoverTracker;

class ToolButton final : public QAbstractButton {
    Q_OBJECT

public:
    static constexpr int kIconSize = 18;
    static constexpr int kBoxSize = 36;

    ToolButton(const QString &iconId, const QString &tip, QWidget *parent = nullptr);

    void setLabel(const QString &label);
    void setIconId(const QString &iconId);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_iconId;
    QString m_label;
    HoverTracker *m_hover;
};

}  // namespace sky
