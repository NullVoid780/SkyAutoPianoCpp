#include "ui/core/easing.hpp"

#include <QPointF>

namespace sky {

namespace {

QEasingCurve bezier(QPointF c1, QPointF c2)
{
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment(c1, c2, QPointF(1, 1));
    return curve;
}

}  // namespace

QEasingCurve easingCurve(Easing easing)
{
    switch (easing) {
    case Easing::Linear:
        return QEasingCurve(QEasingCurve::Linear);
    case Easing::Ease:
        return bezier({0.25, 0.1}, {0.25, 1.0});
    case Easing::EaseOut:
        return bezier({0, 0}, {0.58, 1.0});
    case Easing::EaseInOut:
        return bezier({0.42, 0}, {0.58, 1.0});
    case Easing::ToastIn:
        return bezier({0.215, 0.610}, {0.355, 1.0});
    case Easing::ToastOut:
        return bezier({0.755, 0.050}, {0.855, 0.060});
    }
    return QEasingCurve(QEasingCurve::Linear);
}

}  // namespace sky
