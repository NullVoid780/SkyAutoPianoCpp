#include "ui/widgets/seek_slider.hpp"

#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionSlider>

namespace sky {

SeekSlider::SeekSlider(QWidget *parent) : QSlider(Qt::Horizontal, parent)
{
    setCursor(Qt::PointingHandCursor);
}

int SeekSlider::valueAt(const QPoint &position) const
{
    QStyleOptionSlider option;
    initStyleOption(&option);
    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, this);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
    const int span = groove.width() - handle.width();
    const int offset = position.x() - groove.x() - handle.width() / 2;
    return QStyle::sliderValueFromPosition(minimum(), maximum(), offset, span, option.upsideDown);
}

void SeekSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && maximum() > minimum()) {
        QStyleOptionSlider option;
        initStyleOption(&option);
        const QRect handle = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
        if (!handle.contains(event->position().toPoint())) {
            setValue(valueAt(event->position().toPoint()));
        }
    }
    QSlider::mousePressEvent(event);
}

}  // namespace sky
