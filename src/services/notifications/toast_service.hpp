#pragma once

#include <string>

#include <QObject>

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

class ToastService final : public QObject {
    Q_OBJECT

public:
    explicit ToastService(QObject *parent = nullptr);

    void show(const Toast &toast);
    void show(ToastType type, const std::string &message, int durationMs = kDefaultToastMs);
    void showTitled(ToastType type, const std::string &title, const std::string &message,
                    int durationMs = kDefaultToastMs);

signals:
    void toastRequested(const sky::Toast &toast);
};

}  // namespace sky

Q_DECLARE_METATYPE(sky::Toast)
