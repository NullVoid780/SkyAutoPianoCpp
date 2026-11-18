#pragma once

#include <QAbstractButton>

namespace sky {

class HoverTracker;

class NavTab final : public QAbstractButton {
    Q_OBJECT

public:
    NavTab(const QString &iconId, const QString &label, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_iconId;
    HoverTracker *m_hover;
};

}  // namespace sky
