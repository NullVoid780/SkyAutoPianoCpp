#include "ui/main_window/speed_control.hpp"

#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>

#include <cmath>
#include <functional>

#include "core/json.hpp"
#include "core/string_utils.hpp"

namespace sky {

namespace {

constexpr double kMin = 0.1;
constexpr double kMax = 5.0;
constexpr double kStep = 0.1;

class SpeedField final : public QLineEdit {
public:
    explicit SpeedField(QWidget *parent) : QLineEdit(parent) {}

    std::function<void(double)> onStep;

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if ((event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) && onStep) {
            onStep(event->key() == Qt::Key_Up ? kStep : -kStep);
            return;
        }
        QLineEdit::keyPressEvent(event);
    }
};

QString format(double value)
{
    return toQString(formatJsNumber(value));
}

}  // namespace

SpeedControl::SpeedControl(QWidget *parent) : QWidget(parent)
{
    auto *label = new QLabel(QStringLiteral("Speed:"), this);
    label->setObjectName(QStringLiteral("SpeedLabel"));
    auto *field = new SpeedField(this);
    m_field = field;
    m_field->setObjectName(QStringLiteral("SpeedField"));
    m_field->setFixedSize(40, 20);
    m_field->setValidator(new QDoubleValidator(0, kMax, 2, m_field));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(label);
    layout->addWidget(m_field);

    const auto commit = [this](double value) {
        const double normalizedValue = normalized(value);
        setSpeed(normalizedValue);
        emit speedCommitted(normalizedValue);
    };
    field->onStep = [this, commit](double delta) { commit(speed() + delta); };
    connect(m_field, &QLineEdit::editingFinished, this, [this, commit] { commit(speed()); });
}

double SpeedControl::normalized(double value)
{
    if (!std::isfinite(value)) {
        value = kMin;
    }
    return std::floor(std::clamp(value, kMin, kMax) * 10 + 0.5) / 10;
}

void SpeedControl::setSpeed(double speed)
{
    const QSignalBlocker blocker(m_field);
    m_field->setText(format(std::isfinite(speed) ? speed : 1.0));
}

double SpeedControl::speed() const
{
    bool ok = false;
    const double value = m_field->text().toDouble(&ok);
    return ok ? value : kMin;
}

bool SpeedControl::isEditing() const
{
    return m_field->hasFocus();
}

}  // namespace sky
