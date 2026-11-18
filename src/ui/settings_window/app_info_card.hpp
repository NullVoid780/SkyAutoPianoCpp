#pragma once

#include <QWidget>

namespace sky {

class AppInfoCard final : public QWidget {
    Q_OBJECT

public:
    explicit AppInfoCard(const QString &version, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QWidget *m_logoSpace;
};

}  // namespace sky
