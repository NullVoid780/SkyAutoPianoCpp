#pragma once

#include <string>

#include <QMetaType>

namespace sky {

enum class ToastType { Success = 1, Warning = 2, Error = 3, Info = 4, Neutral = 5 };

inline constexpr int kDefaultToastMs = 5000;
inline constexpr int kProgressToastMs = 3000;
inline constexpr int kStickyToast = 0;

struct Toast {
    ToastType type = ToastType::Info;
    std::string title;
    std::string message;
    int durationMs = kDefaultToastMs;
};

}  // namespace sky

Q_DECLARE_METATYPE(sky::Toast)
