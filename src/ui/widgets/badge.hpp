#pragma once

#include <QWidget>

namespace sky {

class Badge final : public QWidget {
    Q_OBJECT

public:
    explicit Badge(const QString &text, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_text;
};

}  // namespace sky
