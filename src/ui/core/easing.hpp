#pragma once

#include <QEasingCurve>

namespace sky {

enum class Easing { Linear, Ease, EaseOut, EaseInOut, ToastIn, ToastOut };

QEasingCurve easingCurve(Easing easing);

inline constexpr int kDrawerSlideMs = 240;
inline constexpr int kHoverFastMs = 100;
inline constexpr int kHoverMs = 200;
inline constexpr int kHoverSlowMs = 300;
inline constexpr int kLongPressKnobMs = 125;
inline constexpr int kToastEnterMs = 500;
inline constexpr int kToastExitMs = 400;
inline constexpr int kGearSpinDegrees = 72;

}  // namespace sky
