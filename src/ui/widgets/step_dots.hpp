#pragma once

#include <QWidget>

namespace sky {

class StepDots final : public QWidget {
    Q_OBJECT

public:
    explicit StepDots(QWidget *parent = nullptr);

    void set(int count, int current);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_count = 0;
    int m_current = 0;
};

}  // namespace sky
