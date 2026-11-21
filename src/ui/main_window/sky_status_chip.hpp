#pragma once

#include <QString>
#include <QWidget>

namespace sky {

class SkyStatusChip final : public QWidget {
    Q_OBJECT

public:
    enum class Tone { Good, Warning, Idle };

    explicit SkyStatusChip(QWidget *parent = nullptr);

    void setStatus(const QString &text, Tone tone, const QString &tip);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_text;
    Tone m_tone = Tone::Idle;
};

}  // namespace sky
