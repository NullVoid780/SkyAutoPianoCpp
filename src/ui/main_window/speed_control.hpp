#pragma once

#include <QWidget>

class QLineEdit;

namespace sky {

class SpeedControl final : public QWidget {
    Q_OBJECT

public:
    explicit SpeedControl(QWidget *parent = nullptr);

    void setSpeed(double speed);
    double speed() const;
    bool isEditing() const;

    static double normalized(double value);

signals:
    void speedCommitted(double speed);

private:
    QLineEdit *m_field;
};

}  // namespace sky
