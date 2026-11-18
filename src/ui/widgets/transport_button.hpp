#pragma once

#include <QAbstractButton>
#include <QString>

namespace sky {

class HoverTracker;

class TransportButton final : public QAbstractButton {
    Q_OBJECT

public:
    enum class Kind { Skip, Play };

    TransportButton(Kind kind, const QString &iconId, QWidget *parent = nullptr);

    void setContent(const QString &iconId, const QString &label);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Kind m_kind;
    QString m_iconId;
    HoverTracker *m_hover;
};

}  // namespace sky
