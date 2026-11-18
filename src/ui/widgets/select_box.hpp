#pragma once

#include <vector>

#include <QAbstractButton>
#include <QPointer>

namespace sky {

class HoverTracker;

class SelectBox final : public QAbstractButton {
    Q_OBJECT

public:
    struct Option {
        QString label;
        QString description;
    };

    explicit SelectBox(QWidget *parent = nullptr);

    void addOption(const QString &label, const QString &description = QString());
    void setCurrentIndex(int index);
    int currentIndex() const { return m_current; }
    const std::vector<Option> &options() const { return m_options; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

signals:
    void currentIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void openPopup();
    void choose(int index);

    std::vector<Option> m_options;
    int m_current = -1;
    HoverTracker *m_hover;
    QPointer<QWidget> m_popup;
};

}  // namespace sky
