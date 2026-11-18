#pragma once

#include <QWidget>

namespace sky {

class StateCard final : public QWidget {
    Q_OBJECT

public:
    explicit StateCard(QWidget *parent = nullptr);

    void setContent(const QString &title, const QString &body);
    int heightForWidth(int width) const override;
    bool hasHeightForWidth() const override { return true; }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    QString m_body;
};

}  // namespace sky
