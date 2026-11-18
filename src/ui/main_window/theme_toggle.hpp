#pragma once

#include <QAbstractButton>

class QVariantAnimation;

namespace sky {

class HoverTracker;

class ThemeToggle final : public QAbstractButton {
    Q_OBJECT

public:
    explicit ThemeToggle(QWidget *parent = nullptr);

    QSize sizeHint() const override { return QSize(20, 20); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void syncWithTheme(bool animate);

    QVariantAnimation *m_fade;
    HoverTracker *m_hover;
    qreal m_darkness = 0;
};

}  // namespace sky
