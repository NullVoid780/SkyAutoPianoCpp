#pragma once

#include <functional>

#include <QColor>
#include <QWidget>

class QVariantAnimation;

namespace sky {

class ProgressBar final : public QWidget {
    Q_OBJECT

public:
    struct Palette {
        std::function<QColor()> track;
        std::function<QColor()> fillStart;
        std::function<QColor()> fillEnd;
    };

    ProgressBar(Palette palette, QWidget *parent = nullptr);

    void setValue(int percent);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Palette m_palette;
    QVariantAnimation *m_animation;
    qreal m_shown = 0;
    int m_target = 0;
};

}  // namespace sky
