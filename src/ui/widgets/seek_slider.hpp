#pragma once

#include <QSlider>

namespace sky {

class SeekSlider final : public QSlider {
    Q_OBJECT

public:
    explicit SeekSlider(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    int valueAt(const QPoint &position) const;
};

}  // namespace sky
